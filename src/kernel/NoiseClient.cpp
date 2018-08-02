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

	//echo_load_private_key("client_key_25519", client_private_key_25519, sizeof(client_private_key_25519));
	client_private_key = "client_key_25519";

	send_cipher = 0;
	recv_cipher = 0;

	sentId = false;

	handshakeComplete = false;

	log->printf(LOG_LEVEL_INFO, "Noise CLIENT !!!!!! started");
	log->printf(LOG_LEVEL_INFO, std::to_string(NOISE_ACTION_NONE) + ", " + std::to_string(NOISE_ACTION_WRITE_MESSAGE) + ", " + std::to_string(NOISE_ACTION_READ_MESSAGE) + ", " + std::to_string(NOISE_ACTION_FAILED) + ", " + std::to_string(NOISE_ACTION_SPLIT) + ", " + std::to_string(NOISE_ACTION_COMPLETE));

//		/* Check that the echo protocol supports the handshake protocol.
//		   One-way handshake patterns and XXfallback are not yet supported. */
//		std::string protocol = "Noise_NN_25519_AESGCM_SHA256";
//		if (!echo_get_protocol_id(&id, protocol.c_str())) {
//			fprintf(stderr, "%s: not supported by the echo protocol\n", protocol.c_str());
//			//return 1;
//		}
//
//		/* Create a HandshakeState object for the protocol */
//		int err = noise_handshakestate_new_by_name
//			(&handshake, protocol.c_str(), NOISE_ROLE_INITIATOR);
//		if (err != NOISE_ERROR_NONE) {
//			noise_perror(protocol.c_str(), err);
//			//return 1;
//		}
//
//		/* Set the handshake options and verify that everything we need
//		   has been supplied on the command-line. */
//		if (!initialize_handshake(handshake, &id, sizeof(id))) {
//			noise_handshakestate_free(handshake);
//			//return 1;
//		}
//		if (noise_init() != NOISE_ERROR_NONE) {
//			fprintf(stderr, "Noise initialization failed\n");
//			//return 1;
//		}



	if (noise_init() != NOISE_ERROR_NONE) {
		log->printf(LOG_LEVEL_INFO, "Noise initialization failed");
		fprintf(stderr, "Noise initialization failed\n");
		return;
	}

	/* Check that the echo protocol supports the handshake protocol.
	   One-way handshake patterns and XXfallback are not yet supported. */
	std::string protocol = "Noise_XX_25519_AESGCM_SHA256";//"Noise_NN_25519_AESGCM_SHA256";
	if (!getProtocolId(&id, protocol.c_str())) {
		log->printf(LOG_LEVEL_INFO, protocol + " not supported by the echo protocol\n");
		fprintf(stderr, "%s: not supported by the echo protocol\n", protocol.c_str());
		return;
	}

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

	while(true) {
		if(!sentId) {
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
				//handshakeMutex.unlock();
				/*if (!echo_send(fd, message, mbuf.size + 2)) {
					ok = 0;
					break;
				}*/

				log->printf(LOG_LEVEL_INFO, "CLIENT WRITE INFO REALLY SENDING A PACKET FRIEND");
				sf::Packet packet;
				packet.append(message, mbuf.size + 2);
				server->send(packet);
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

	/* If we will be padding messages, we will need a random number generator */
	/*if (ok && padding) {
		err = noise_randstate_new(&rand);
		if (err != NOISE_ERROR_NONE) {
			noise_perror("random number generator", err);
			ok = 0;
		}
	}*/

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
		//std::lock_guard<std::mutex> hm(handshakeMutex);

		/* Read the next handshake message and discard the payload */
		//sf::Packet packet;
		//server->receive(packet);

		message_size = packet.getDataSize();
		memcpy(message, packet.getData(), packet.getDataSize());
		/*message_size = echo_recv(fd, message, sizeof(message));
		if (!message_size) {
			ok = 0;
			break;
		}*/

		noise_buffer_set_input(mbuf, message + 2, message_size - 2);
		err = noise_handshakestate_read_message(handshake, &mbuf, NULL);
		if (err != NOISE_ERROR_NONE) {
			noise_perror("read handshake", err);
			ok = 0;
			//break;
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

//	    /* Set the PSK if one is present.  This will fail if a PSK is not needed.
//	       If a PSK is needed but it wasn't provided then the protocol will
//	       fail later when noise_handshakestate_start() is called. */
//	    if (psk_file && noise_handshakestate_needs_pre_shared_key(handshake)) {
//	        if (!echo_load_public_key(psk_file.c_str(), (unsigned char*)psk.c_str(), sizeof(psk.c_str())))
//	            return 0;
//	        err = noise_handshakestate_set_pre_shared_key
//	            (handshake, (const unsigned char*)psk.c_str(), sizeof(psk.c_str()));
//	        if (err != NOISE_ERROR_NONE) {
//	            noise_perror("psk", err);
//	            return 0;
//	        }
//	    }
//
//	    /* Set the local keypair for the client */
	if (noise_handshakestate_needs_local_keypair(handshake)) {
		if (client_private_key.length() > 0) {
			dh = noise_handshakestate_get_local_keypair_dh(handshake);
			key_len = noise_dhstate_get_private_key_length(dh);
			key = (uint8_t *)malloc(key_len);
			if(!key) {
				log->printf(LOG_LEVEL_INFO, "CLIENT KEY ERROR");
				return 0;
			}
			if(noiseUtil.loadPrivateKey(client_private_key.c_str(), key, key_len)) {
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
//
//	    /* Set the remote public key for the server */
//	    if (noise_handshakestate_needs_remote_public_key(handshake)) {
//	        if (server_public_key) {
//	            dh = noise_handshakestate_get_remote_public_key_dh(handshake);
//	            key_len = noise_dhstate_get_public_key_length(dh);
//	            key = (uint8_t *)malloc(key_len);
//	            if (!key)
//	                return 0;
//	            if (!echo_load_public_key(server_public_key.c_str(), key, key_len)) {
//	                noise_free(key, key_len);
//	                return 0;
//	            }
//	            err = noise_dhstate_set_public_key(dh, key, key_len);
//	            noise_free(key, key_len);
//	            if (err != NOISE_ERROR_NONE) {
//	                noise_perror("set server public key", err);
//	                return 0;
//	            }
//	        } else {
//	            fprintf(stderr, "Server public key required, but not provided.\n");
//	            return 0;
//	        }
//	    }

	/* Ready to go */
	log->printf(LOG_LEVEL_INFO, "handshake initialization successful!");
	return 1;
}

/* Convert a Noise handshake protocol name into an Echo protocol id */
int NoiseClient::getProtocolId(EchoProtocolId* id, const char* name) {
	NoiseProtocolId nid;
	int ok = 1;
	memset(id, 0, sizeof(EchoProtocolId));
	if (noise_protocol_name_to_id(&nid, name, strlen(name)) != NOISE_ERROR_NONE)
		return 0;

	switch (nid.prefix_id) {
	case NOISE_PREFIX_STANDARD:     id->psk = ECHO_PSK_DISABLED; break;
	case NOISE_PREFIX_PSK:          id->psk = ECHO_PSK_ENABLED; break;
	default:                        ok = 0; break;
	}

	switch (nid.pattern_id) {
	case NOISE_PATTERN_NN:          id->pattern = ECHO_PATTERN_NN; break;
	case NOISE_PATTERN_KN:          id->pattern = ECHO_PATTERN_KN; break;
	case NOISE_PATTERN_NK:          id->pattern = ECHO_PATTERN_NK; break;
	case NOISE_PATTERN_KK:          id->pattern = ECHO_PATTERN_KK; break;
	case NOISE_PATTERN_NX:          id->pattern = ECHO_PATTERN_NX; break;
	case NOISE_PATTERN_KX:          id->pattern = ECHO_PATTERN_KX; break;
	case NOISE_PATTERN_XN:          id->pattern = ECHO_PATTERN_XN; break;
	case NOISE_PATTERN_IN:          id->pattern = ECHO_PATTERN_IN; break;
	case NOISE_PATTERN_XK:          id->pattern = ECHO_PATTERN_XK; break;
	case NOISE_PATTERN_IK:          id->pattern = ECHO_PATTERN_IK; break;
	case NOISE_PATTERN_XX:          id->pattern = ECHO_PATTERN_XX; break;
	case NOISE_PATTERN_IX:          id->pattern = ECHO_PATTERN_IX; break;
	case NOISE_PATTERN_NN_HFS:      id->pattern = ECHO_PATTERN_NN | ECHO_PATTERN_HFS; break;
	case NOISE_PATTERN_KN_HFS:      id->pattern = ECHO_PATTERN_KN | ECHO_PATTERN_HFS; break;
	case NOISE_PATTERN_NK_HFS:      id->pattern = ECHO_PATTERN_NK | ECHO_PATTERN_HFS; break;
	case NOISE_PATTERN_KK_HFS:      id->pattern = ECHO_PATTERN_KK | ECHO_PATTERN_HFS; break;
	case NOISE_PATTERN_NX_HFS:      id->pattern = ECHO_PATTERN_NX | ECHO_PATTERN_HFS; break;
	case NOISE_PATTERN_KX_HFS:      id->pattern = ECHO_PATTERN_KX | ECHO_PATTERN_HFS; break;
	case NOISE_PATTERN_XN_HFS:      id->pattern = ECHO_PATTERN_XN | ECHO_PATTERN_HFS; break;
	case NOISE_PATTERN_IN_HFS:      id->pattern = ECHO_PATTERN_IN | ECHO_PATTERN_HFS; break;
	case NOISE_PATTERN_XK_HFS:      id->pattern = ECHO_PATTERN_XK | ECHO_PATTERN_HFS; break;
	case NOISE_PATTERN_IK_HFS:      id->pattern = ECHO_PATTERN_IK | ECHO_PATTERN_HFS; break;
	case NOISE_PATTERN_XX_HFS:      id->pattern = ECHO_PATTERN_XX | ECHO_PATTERN_HFS; break;
	case NOISE_PATTERN_IX_HFS:      id->pattern = ECHO_PATTERN_IX | ECHO_PATTERN_HFS; break;
	default:                        ok = 0; break;
	}

	switch (nid.cipher_id) {
	case NOISE_CIPHER_CHACHAPOLY:   id->cipher = ECHO_CIPHER_CHACHAPOLY; break;
	case NOISE_CIPHER_AESGCM:       id->cipher = ECHO_CIPHER_AESGCM; break;
	default:                        ok = 0; break;
	}

	switch (nid.dh_id) {
	case NOISE_DH_CURVE25519:       id->dh = ECHO_DH_25519; break;
	case NOISE_DH_CURVE448:         id->dh = ECHO_DH_448; break;
	case NOISE_DH_NEWHOPE:          id->dh = ECHO_DH_NEWHOPE; break;
	default:                        ok = 0; break;
	}

	switch (nid.hybrid_id) {
	case NOISE_DH_CURVE25519:       id->dh |= ECHO_HYBRID_25519; break;
	case NOISE_DH_CURVE448:         id->dh |= ECHO_HYBRID_448; break;
	case NOISE_DH_NEWHOPE:          id->dh |= ECHO_HYBRID_NEWHOPE; break;
	case NOISE_DH_NONE:             break;
	default:                        ok = 0; break;
	}

	switch (nid.hash_id) {
	case NOISE_HASH_SHA256:         id->hash = ECHO_HASH_SHA256; break;
	case NOISE_HASH_SHA512:         id->hash = ECHO_HASH_SHA512; break;
	case NOISE_HASH_BLAKE2s:        id->hash = ECHO_HASH_BLAKE2s; break;
	case NOISE_HASH_BLAKE2b:        id->hash = ECHO_HASH_BLAKE2b; break;
	default:                        ok = 0; break;
	}

	return ok;
}

NoiseClient::~NoiseClient() {
	writeInfoThread->join();
}

