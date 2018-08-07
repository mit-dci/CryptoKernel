/*
 * NoiseServer.cpp
 *
 *  Created on: Aug 2, 2018
 *      Author: luke
 */

#include "NoiseServer.h"

NoiseServer::NoiseServer(std::shared_ptr<sf::TcpSocket> client, uint64_t port, CryptoKernel::Log* log) {
	send_cipher = 0;
	recv_cipher = 0;

	this->client = client;
	this->log = log;
	this->port = port;

	handshake = 0;
	message_size = 0;
	receivedId = false;
	receivedPubKey = false;

	handshakeComplete = false;
	handshakeSuccess = false;

	if(noise_init() != NOISE_ERROR_NONE) {
		log->printf(LOG_LEVEL_WARN, "Noise(): Server, noise initialization failed.");
		//fprintf(stderr, "Noise initialization failed\n");
		return;
	}

	if(!noiseUtil.loadPrivateKey("keys/server_key_25519", server_key_25519, sizeof(server_key_25519))) {
		log->printf(LOG_LEVEL_INFO, "could not load server key 25519");
		uint8_t* server_pub_key;
		uint8_t* server_priv_key;
		if(!noiseUtil.writeKeys("keys/server_key_25519.pub", "keys/server_key_25519", &server_pub_key, &server_priv_key)) {
			log->printf(LOG_LEVEL_INFO, "Could not write server keys.");
			setHandshakeComplete(true, false);
			return;
		}
		memcpy(server_key_25519, server_priv_key, CURVE25519_KEY_LEN); // put the new key in its proper place
		delete server_pub_key;
		delete server_priv_key;
	}

	writeInfoThread.reset(new std::thread(&NoiseServer::writeInfo, this)); // start the write info thread
}

void NoiseServer::writeInfo() {
	log->printf(LOG_LEVEL_INFO, "SERVER write info starting");

	uint8_t message[MAX_MESSAGE_LEN + 2];

	while(!getHandshakeComplete()) { // it might fail in another thread, and so become "complete"
		handshakeMutex.lock();
		int action = noise_handshakestate_get_action(handshake);
		handshakeMutex.unlock();
		if (action == NOISE_ACTION_WRITE_MESSAGE) {
			log->printf(LOG_LEVEL_INFO, "Noise(): Server writing message.");
			/* Write the next handshake message with a zero-length payload */
			noise_buffer_set_output(mbuf, message + 2, sizeof(message) - 2);
			handshakeMutex.lock();
			int err = noise_handshakestate_write_message(handshake, &mbuf, NULL);
			handshakeMutex.unlock();
			if (err != NOISE_ERROR_NONE) {
				//log->printf(LOG_LEVEL_WARN, "Noise(): Server, error writing message: " + noiseUtil.errToString(err));
				setHandshakeComplete(true, false);
				return;
			}
			message[0] = (uint8_t)(mbuf.size >> 8);
			message[1] = (uint8_t)mbuf.size;

			sf::Packet packet;
			packet.append(message, mbuf.size + 2);

			if(client->send(packet) != sf::Socket::Done) {
				log->printf(LOG_LEVEL_WARN, "Noise(): Server, something went wrong sending packet to client.");
				setHandshakeComplete(true, false);
				return;
			}
		}
		else if(action != NOISE_ACTION_READ_MESSAGE && action != NOISE_ACTION_NONE) { // for some reason, a noise action none fires
			break;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(50)); // totally arbitrary
	}

	int ok = 1;
	/* If the action is not "split", then the handshake has failed */
	handshakeMutex.lock();
	if (noise_handshakestate_get_action(handshake) != NOISE_ACTION_SPLIT) {
		log->printf(LOG_LEVEL_INFO, "Noise(): Server, handshake failed.");
		ok = 0;
	}
	handshakeMutex.unlock();

	/* Split out the two CipherState objects for send and receive */
	int err;
	if (ok) {
		handshakeMutex.lock();
		err = noise_handshakestate_split(handshake, &send_cipher, &recv_cipher);
		if (err != NOISE_ERROR_NONE) {
			//log->printf(LOG_LEVEL_WARN, "Noise(): Server, split failed: " + noiseUtil.errToString(err));
			ok = 0;
		}
		handshakeMutex.unlock();
	}

	/* We no longer need the HandshakeState */
	noise_handshakestate_free(handshake);
	handshake = 0;

	setHandshakeComplete(true, ok);
}

void NoiseServer::receivePacket(sf::Packet packet) {
	int err;
	log->printf(LOG_LEVEL_INFO, "Noise(): Server, receiving a packet.");
	uint8_t message[MAX_MESSAGE_LEN + 2];

	if(!receivedPubKey) {
		log->printf(LOG_LEVEL_INFO, "received a packet, hopefully the public key!!");
		memcpy(client_key_25519, packet.getData(), packet.getDataSize());
		receivedPubKey = true;
	}
	else if(!receivedId) {
		log->printf(LOG_LEVEL_INFO, "Noise(): Server, id pattern size is " + std::to_string(packet.getDataSize()));
		memcpy(&id, packet.getData(), (unsigned long int)packet.getDataSize());

		log->printf(LOG_LEVEL_INFO, "Noise(): Server, id pattern itself is " + std::to_string(id.pattern));
		receivedId = true;

		/* Convert the echo protocol identifier into a Noise protocol identifier */
		bool ok = true;
		NoiseProtocolId nid;
		if (ok && !noiseUtil.toNoiseProtocolId(&nid, &id)) {
			log->printf(LOG_LEVEL_WARN, "Noise(): Unknown echo protocol identifier");
			//fprintf(stderr, "Unknown echo protocol identifier\n");
			setHandshakeComplete(true, false);
			return;
		}

		/* Create a HandshakeState object to manage the server's handshake */
		if (ok) {
			std::lock_guard<std::mutex> hc(handshakeMutex);
			err = noise_handshakestate_new_by_id
				(&handshake, &nid, NOISE_ROLE_RESPONDER);
			if (err != NOISE_ERROR_NONE) {
				//log->printf(LOG_LEVEL_INFO, "Noise(): Server, create handshake: " + noiseUtil.errToString(err));
				setHandshakeComplete(true, false);
				return;
			}
		}

		/* Set all keys that are needed by the client's requested echo protocol */
		if (ok) {
			std::lock_guard<std::mutex> hm(handshakeMutex);
			if (!initializeHandshake(handshake, &nid, &id, sizeof(id))) {
				log->printf(LOG_LEVEL_WARN, "Noise(): Server, couldn't initialize handshake");
				setHandshakeComplete(true, false);
				return;
			}
		}

		/* Start the handshake */
		if (ok) {
			std::lock_guard<std::mutex> hc(handshakeMutex);
			err = noise_handshakestate_start(handshake);
			if (err != NOISE_ERROR_NONE) {
				//log->printf(LOG_LEVEL_INFO, "Noise(): Server, couldn't start handshake: " + noiseUtil.errToString(err));
				setHandshakeComplete(true, false);
				return;
			}
		}
	}
	else {
		std::lock_guard<std::mutex> hm(handshakeMutex);
		int action = noise_handshakestate_get_action(handshake);
		if(action == NOISE_ACTION_READ_MESSAGE) {
			log->printf(LOG_LEVEL_INFO, "Noise(): Server, reading message.");
			message_size = packet.getDataSize();
			memcpy(message, packet.getData(), packet.getDataSize());

			noise_buffer_set_input(mbuf, message + 2, message_size - 2);
			err = noise_handshakestate_read_message(handshake, &mbuf, NULL);
			if (err != NOISE_ERROR_NONE) {
				//log->printf(LOG_LEVEL_WARN, "Noise(): Server, read handshake error: " + noiseUtil.errToString(err));
				setHandshakeComplete(true, false);
				return;
			}
		}
	}
}

/* Initialize's the handshake with all necessary keys */
int NoiseServer::initializeHandshake(NoiseHandshakeState* handshake, const NoiseProtocolId* nid, const void* prologue, size_t prologue_len) {
	NoiseDHState *dh;
	int dh_id;
	int err;

	/* Set the prologue first */
	err = noise_handshakestate_set_prologue(handshake, prologue, prologue_len);
	if (err != NOISE_ERROR_NONE) {
		//log->printf(LOG_LEVEL_WARN, "Noise(): Server, prologue error: " + noiseUtil.errToString(err));
		return 0;
	}

	/* Set the local keypair for the server based on the DH algorithm */
	if(noise_handshakestate_needs_local_keypair(handshake)) {
		dh = noise_handshakestate_get_local_keypair_dh(handshake);
		dh_id = noise_dhstate_get_dh_id(dh);
		if (dh_id == NOISE_DH_CURVE25519) {
			err = noise_dhstate_set_keypair_private
				(dh, server_key_25519, sizeof(server_key_25519));
		}
		else {
			err = NOISE_ERROR_UNKNOWN_ID;
		}
		if (err != NOISE_ERROR_NONE) {
			//log->printf(LOG_LEVEL_WARN, "Noise(): Server, set private key error, " + noiseUtil.errToString(err));
			return 0;
		}
	}

	/* Set the remote public key for the client */
	if(noise_handshakestate_needs_remote_public_key(handshake)) {
		dh = noise_handshakestate_get_remote_public_key_dh(handshake);
		dh_id = noise_dhstate_get_dh_id(dh);
		if (dh_id == NOISE_DH_CURVE25519) {
			err = noise_dhstate_set_public_key
				(dh, client_key_25519, sizeof(client_key_25519));
		}
		else {
			err = NOISE_ERROR_UNKNOWN_ID;
		}
		if (err != NOISE_ERROR_NONE) {
			//log->printf(LOG_LEVEL_WARN, "Noise(): Server, set public key error, " + noiseUtil.errToString(err));
			return 0;
		}
	}

	/* Ready to go */
	return 1;
}

void NoiseServer::setHandshakeComplete(bool complete, bool success) {
	if(success) {
		log->printf(LOG_LEVEL_INFO, "Noise(): Server handshake succeeded");
	}

	std::lock_guard<std::mutex> hcm(handshakeCompleteMutex);
	handshakeComplete = complete;
	handshakeSuccess = success;
}

bool NoiseServer::getHandshakeSuccess() {
	std::lock_guard<std::mutex> hcm(handshakeCompleteMutex);
	return handshakeSuccess;
}

bool NoiseServer::getHandshakeComplete() {
	std::lock_guard<std::mutex> hcm(handshakeCompleteMutex);
	return handshakeComplete;
}

NoiseServer::~NoiseServer() {
	log->printf(LOG_LEVEL_INFO, "Cleaning up noise server");
	if(!getHandshakeComplete()) {
		setHandshakeComplete(true, false);
	}
	writeInfoThread->join();
}
