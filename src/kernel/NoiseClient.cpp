/*
 * NoiseClient.cpp
 *
 *  Created on: Aug 2, 2018
 *      Author: luke
 */

#include "NoiseClient.h"

NoiseClient::NoiseClient(sf::TcpSocket* server, std::string ipAddress, uint64_t port, CryptoKernel::Log* log) {
	this->server = server;
	this->log = log;
	this->ipAddress = ipAddress;
	this->port = port;

	client_private_key = "keys/client_key_25519";

	send_cipher = 0;
	recv_cipher = 0;

	sentId = false;

	handshakeComplete = false;

	log->printf(LOG_LEVEL_INFO, "Noise CLIENT !!!!!! started");
	log->printf(LOG_LEVEL_INFO, std::to_string(NOISE_ACTION_NONE) + ", " + std::to_string(NOISE_ACTION_WRITE_MESSAGE) + ", " + std::to_string(NOISE_ACTION_READ_MESSAGE) + ", " + std::to_string(NOISE_ACTION_FAILED) + ", " + std::to_string(NOISE_ACTION_SPLIT) + ", " + std::to_string(NOISE_ACTION_COMPLETE));

	if (noise_init() != NOISE_ERROR_NONE) {
		log->printf(LOG_LEVEL_INFO, "Noise initialization failed");
		fprintf(stderr, "Noise initialization failed\n");
		return;
	}

	/* Check that the echo protocol supports the handshake protocol.
	   One-way handshake patterns and XXfallback are not yet supported. */
	std::string protocol = "Noise_XX_25519_AESGCM_SHA256";//"Noise_NN_25519_AESGCM_SHA256";

	// in the Echo example, this is accomplished by parsing the protocol string (ie 'Noise_XX_25519_AESGCM_SHA256') in echo_get_protocol_id
	id.pattern = ECHO_PATTERN_XX;
	id.psk = ECHO_PSK_ENABLED;
	id.cipher = ECHO_CIPHER_AESGCM;
	id.hash = ECHO_HASH_SHA256;
	id.dh = ECHO_DH_25519;

	/* Create a HandshakeState object for the protocol */
	int err = noise_handshakestate_new_by_name(&handshake, protocol.c_str(), NOISE_ROLE_INITIATOR);
	if (err != NOISE_ERROR_NONE) {
		log->printf(LOG_LEVEL_INFO, "Noise error, line 76!");
		noise_perror(protocol.c_str(), err);
		return;
	}

	/* Set the handshake options and verify that everything we need
	   has been supplied on the command-line. */
	if (!initializeHandshake(handshake, &id, sizeof(id))) {
		log->printf(LOG_LEVEL_INFO, "NOISE ERROR, LINE 84");
		noise_handshakestate_free(handshake);
		return;
	}

	log->printf(LOG_LEVEL_INFO, "CLIENT Well, we got through the constructor.  Starting writeInfo.");
	writeInfoThread.reset(new std::thread(&NoiseClient::writeInfo, this)); // start the write info thread
}

void NoiseClient::writeInfo() {
	log->printf(LOG_LEVEL_INFO, "CLIENT writing info");

	int action;
	NoiseBuffer mbuf;
	int err, ok;
	size_t message_size;

	uint8_t message[MAX_MESSAGE_LEN + 2];

	ok = 1;

	bool sentPubKey = false;

	while(true) {
		if(!sentPubKey) { // we need to share our public key over the network
			log->printf(LOG_LEVEL_INFO, "sending public key to " + server->getRemoteAddress().toString());
			sf::Packet pubKeyPacket;
			if(!noiseUtil.loadPublicKey("keys/client_key_25519.pub", clientKey25519, sizeof(clientKey25519))) {
				log->printf(LOG_LEVEL_ERR, "Could not load public key");
				ok = 0;
				break;
			}
			pubKeyPacket.append(clientKey25519, sizeof(clientKey25519));

			if(server->send(pubKeyPacket) != sf::Socket::Done) {
				continue; // keep sending the public key until it goes through
			}
			sentPubKey = true;
		}
		else if(!sentId) {
			sf::Packet idPacket;
			idPacket.append(&id, sizeof(id));
			log->printf(LOG_LEVEL_INFO, "CLIENT appended " + std::to_string(sizeof(id)) + " bytes to packet for id");
			if(server->send(idPacket) != sf::Socket::Done) {
				continue; // keep sending the id until it goes through, necessary because we're not blocking
			}

			err = noise_handshakestate_start(handshake);
			if (err != NOISE_ERROR_NONE) {
				log->printf(LOG_LEVEL_INFO, "CLIENT start handshake failed");
				noise_perror("start handshake", err);
				ok = 0;
			}
			sentId = true;
		}
		else {
			//handshakeMutex.lock();
			action = noise_handshakestate_get_action(handshake);
			//handshakeMutex.unlock();

			if(action == NOISE_ACTION_WRITE_MESSAGE) {
				log->printf(LOG_LEVEL_INFO, "CLIENT Well, waddya know, I'm gonna send a message, I thinK!");
				/* Write the next handshake message with a zero-length payload */
				//handshakeMutex.lock();
				noise_buffer_set_output(mbuf, message + 2, sizeof(message) - 2);
				err = noise_handshakestate_write_message(handshake, &mbuf, NULL);
				if (err != NOISE_ERROR_NONE) {
					log->printf(LOG_LEVEL_INFO, "CLIENT WRITE HANDSHAKE FAILED");
					//handshakeMutex.unlock();
					noise_perror("write handshake", err);
					ok = 0;
					//break;
					return;
				}
				message[0] = (uint8_t)(mbuf.size >> 8);
				message[1] = (uint8_t)mbuf.size;

				log->printf(LOG_LEVEL_INFO, "CLIENT WRITE INFO REALLY SENDING A PACKET FRIEND");
				sf::Packet packet;
				packet.append(message, mbuf.size + 2);
				if(server->send(packet) != sf::Socket::Done) {
					log->printf(LOG_LEVEL_ERR, "CLIENT couldn't send packet");
					ok = 0;
					break;
				}
			}
			else if(action != NOISE_ACTION_READ_MESSAGE) {
				log->printf(LOG_LEVEL_INFO, "Either the handshake succeeded, or it failed");
				break;
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(50)); // again, totally arbitrary
	}

	/* If the action is not "split", then the handshake has failed */
	if (ok && noise_handshakestate_get_action(handshake) != NOISE_ACTION_SPLIT) {
		fprintf(stderr, "protocol handshake failed\n");
		log->printf(LOG_LEVEL_INFO, "CLIENT protocol handshake failed :(");
		ok = 0;
	}

	/* Split out the two CipherState objects for send and receive */
	if (ok) {
		err = noise_handshakestate_split(handshake, &send_cipher, &recv_cipher);
		if (err != NOISE_ERROR_NONE) {
			log->printf(LOG_LEVEL_INFO, "CLIENT error split to start data transfer");
			noise_perror("split to start data transfer", err);
			ok = 0;
		}
	}

	/* We no longer need the HandshakeState */
	noise_handshakestate_free(handshake);
	handshake = 0;

	// padding would go here, if we used it

	/* Tell the user that the handshake has been successful */
	if (ok) {
		log->printf(LOG_LEVEL_INFO, "CLIENT Handshake complete!!");
	}
	else {
		log->printf(LOG_LEVEL_INFO, "CLIENT ok is 0, somehow, not sure where..., handshake failed");
	}

	setHandshakeComplete(true);
}

void NoiseClient::recievePacket(sf::Packet packet) {
	log->printf(LOG_LEVEL_INFO, "CLIENT recieving packet");

	//handshakeMutex.lock();
	int action = noise_handshakestate_get_action(handshake);
	//handshakeMutex.unlock();
	NoiseBuffer mbuf;
	int err, ok;
	size_t message_size;

	uint8_t message[MAX_MESSAGE_LEN + 2];

	if(action == NOISE_ACTION_READ_MESSAGE) {
		log->printf(LOG_LEVEL_INFO, "CLIENT READING A MESSAGE!!");

		message_size = packet.getDataSize();
		memcpy(message, packet.getData(), packet.getDataSize());

		noise_buffer_set_input(mbuf, message + 2, message_size - 2);
		err = noise_handshakestate_read_message(handshake, &mbuf, NULL);
		if (err != NOISE_ERROR_NONE) {
			noise_perror("read handshake", err);
			ok = 0;
			return;
		}
	}
}

void NoiseClient::setHandshakeComplete(bool complete) {
	std::lock_guard<std::mutex> hcm(handshakeCompleteMutex);
	handshakeComplete = complete;
}

bool NoiseClient::getHandshakeComplete() {
	std::lock_guard<std::mutex> hcm(handshakeCompleteMutex);
	return handshakeComplete;
}

/* Initialize's the handshake using command-line options */
int NoiseClient::initializeHandshake(NoiseHandshakeState *handshake, const void *prologue, size_t prologue_len) {
	NoiseDHState *dh;
	uint8_t *key = 0;
	size_t key_len = 0;
	int err;

	/* Set the prologue first */
	err = noise_handshakestate_set_prologue(handshake, prologue, prologue_len);
	if (err != NOISE_ERROR_NONE) {
		log->printf(LOG_LEVEL_INFO, "CLIENT PROLOGUE FAILED");
		noise_perror("prologue", err);
		return 0;
	}

	// we are not using pre-shared keys, but they would be initialized right here

	/* Set the local keypair for the client */
	if (noise_handshakestate_needs_local_keypair(handshake)) {
		if (client_private_key.length() > 0) {
			dh = noise_handshakestate_get_local_keypair_dh(handshake);
			key_len = noise_dhstate_get_private_key_length(dh);
			key = (uint8_t *)malloc(key_len);
			if(!key) {
				log->printf(LOG_LEVEL_INFO, "CLIENT KEY ERROR");
				return 0;
			}
			if(!noiseUtil.loadPrivateKey(client_private_key.c_str(), key, key_len)) {
				noise_free(key, key_len);
				return 0;
			}
			err = noise_dhstate_set_keypair_private(dh, key, key_len);
			noise_free(key, key_len);
			if (err != NOISE_ERROR_NONE) {
				log->printf(LOG_LEVEL_INFO, "SET CLIENT PRIVATE KEY ERROR");
				noise_perror("set client private key", err);
				return 0;
			}
		} else {
			log->printf(LOG_LEVEL_INFO, "CLIENT private key required but not provided!!");
			fprintf(stderr, "Client private key required, but not provided.\n");
			return 0;
		}
	}

	// we are not using a remote public key for the server, but it would be initialized right here

	/* Ready to go */
	log->printf(LOG_LEVEL_INFO, "handshake initialization successful!");
	return 1;
}

NoiseClient::~NoiseClient() {
	writeInfoThread->join();
}

