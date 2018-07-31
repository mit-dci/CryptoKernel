/*
 * NoiseConnection.h
 *
 *  Created on: Jul 20, 2018
 *      Author: luke
 */

#ifndef NOISECONNECTION_H_
#define NOISECONNECTION_H_

#include <noise/protocol.h>
#include <stdio.h>
#include <string.h>
#include <SFML/Network.hpp>
#include "log.h"

#define CURVE25519_KEY_LEN 32
#define CURVE448_KEY_LEN 56
#define MAX_DH_KEY_LEN 2048

#define ECHO_PSK_DISABLED           0x00
#define ECHO_PSK_ENABLED            0x01

#define ECHO_PATTERN_NN             0x00
#define ECHO_PATTERN_KN             0x01
#define ECHO_PATTERN_NK             0x02
#define ECHO_PATTERN_KK             0x03
#define ECHO_PATTERN_NX             0x04
#define ECHO_PATTERN_KX             0x05
#define ECHO_PATTERN_XN             0x06
#define ECHO_PATTERN_IN             0x07
#define ECHO_PATTERN_XK             0x08
#define ECHO_PATTERN_IK             0x09
#define ECHO_PATTERN_XX             0x0A
#define ECHO_PATTERN_IX             0x0B
#define ECHO_PATTERN_HFS            0x80

#define ECHO_CIPHER_CHACHAPOLY      0x00
#define ECHO_CIPHER_AESGCM          0x01

#define ECHO_DH_25519               0x00
#define ECHO_DH_448                 0x01
#define ECHO_DH_NEWHOPE             0x02
#define ECHO_DH_MASK                0x0F

#define ECHO_HYBRID_NONE            0x00
#define ECHO_HYBRID_25519           0x10
#define ECHO_HYBRID_448             0x20
#define ECHO_HYBRID_NEWHOPE         0x30
#define ECHO_HYBRID_MASK            0xF0

#define ECHO_HASH_SHA256            0x00
#define ECHO_HASH_SHA512            0x01
#define ECHO_HASH_BLAKE2s           0x02
#define ECHO_HASH_BLAKE2b           0x03

typedef struct
{
    uint8_t psk;
    uint8_t pattern;
    uint8_t cipher;
    uint8_t dh;
    uint8_t hash;

} EchoProtocolId;

/* Message buffer for send/receive */
#define MAX_MESSAGE_LEN 4096
static uint8_t message[MAX_MESSAGE_LEN + 2];

class NoiseConnectionClient {
public:
	/* Loads a binary private key from a file.  Returns non-zero if OK. */
	int echo_load_private_key(const char *filename, uint8_t *key, size_t len)
	{
		FILE *file = fopen(filename, "rb");
		size_t posn = 0;
		int ch;
		if (len > MAX_DH_KEY_LEN) {
			fprintf(stderr, "private key length is not supported\n");
			return 0;
		}
		if (!file) {
			perror(filename);
			return 0;
		}
		while ((ch = getc(file)) != EOF) {
			if (posn >= len) {
				fclose(file);
				fprintf(stderr, "%s: private key value is too long\n", filename);
				return 0;
			}
			key[posn++] = (uint8_t)ch;
		}
		if (posn < len) {
			fclose(file);
			fprintf(stderr, "%s: private key value is too short\n", filename);
			return 0;
		}
		fclose(file);
		return 1;
	}

	/* Loads a base64-encoded public key from a file.  Returns non-zero if OK. */
	int echo_load_public_key(const char *filename, uint8_t *key, size_t len)
	{
		FILE *file = fopen(filename, "rb");
		uint32_t group = 0;
		size_t group_size = 0;
		uint32_t digit = 0;
		size_t posn = 0;
		int ch;
		if (len > MAX_DH_KEY_LEN) {
			fprintf(stderr, "public key length is not supported\n");
			return 0;
		}
		if (!file) {
			perror(filename);
			return 0;
		}
		while ((ch = getc(file)) != EOF) {
			if (ch >= 'A' && ch <= 'Z') {
				digit = ch - 'A';
			} else if (ch >= 'a' && ch <= 'z') {
				digit = ch - 'a' + 26;
			} else if (ch >= '0' && ch <= '9') {
				digit = ch - '0' + 52;
			} else if (ch == '+') {
				digit = 62;
			} else if (ch == '/') {
				digit = 63;
			} else if (ch == '=') {
				break;
			} else if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
				fclose(file);
				fprintf(stderr, "%s: invalid character in public key file\n", filename);
				return 0;
			}
			group = (group << 6) | digit;
			if (++group_size >= 4) {
				if ((len - posn) < 3) {
					fclose(file);
					fprintf(stderr, "%s: public key value is too long\n", filename);
					return 0;
				}
				group_size = 0;
				key[posn++] = (uint8_t)(group >> 16);
				key[posn++] = (uint8_t)(group >> 8);
				key[posn++] = (uint8_t)group;
			}
		}
		if (group_size == 3) {
			if ((len - posn) < 2) {
				fclose(file);
				fprintf(stderr, "%s: public key value is too long\n", filename);
				return 0;
			}
			key[posn++] = (uint8_t)(group >> 10);
			key[posn++] = (uint8_t)(group >> 2);
		} else if (group_size == 2) {
			if ((len - posn) < 1) {
				fclose(file);
				fprintf(stderr, "%s: public key value is too long\n", filename);
				return 0;
			}
			key[posn++] = (uint8_t)(group >> 4);
		}
		if (posn < len) {
			fclose(file);
			fprintf(stderr, "%s: public key value is too short\n", filename);
			return 0;
		}
		fclose(file);
		return 1;
	}

	NoiseConnectionClient(sf::TcpSocket* server, std::string ipAddress, uint64_t port, CryptoKernel::Log* log) {
		this->server = server;
		this->log = log;
		this->ipAddress = ipAddress;
		this->port = port;

		send_cipher = 0;
		recv_cipher = 0;

		sentId = false;

		handshakeComplete = false;

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
			fprintf(stderr, "Noise initialization failed\n");
			return;
		}

		/* Check that the echo protocol supports the handshake protocol.
		   One-way handshake patterns and XXfallback are not yet supported. */
		std::string protocol = "Noise_NN_25519_AESGCM_SHA256";
		if (!echo_get_protocol_id(&id, protocol.c_str())) {
			fprintf(stderr, "%s: not supported by the echo protocol\n", protocol.c_str());
			return;
		}

		/* Create a HandshakeState object for the protocol */
		int err = noise_handshakestate_new_by_name
			(&handshake, protocol.c_str(), NOISE_ROLE_INITIATOR);
		if (err != NOISE_ERROR_NONE) {
			noise_perror(protocol.c_str(), err);
			return;
		}

		/* Set the handshake options and verify that everything we need
		   has been supplied on the command-line. */
		if (!initialize_handshake(handshake, &id, sizeof(id))) {
			noise_handshakestate_free(handshake);
			return;
		}

		writeInfoThread.reset(new std::thread(&NoiseConnectionClient::writeInfo, this)); // start the write info thread
	}

	~NoiseConnectionClient() {
		writeInfoThread->join();
	}

	void writeInfo() {
		log->printf(LOG_LEVEL_INFO, "CLIENT writing info");

		int action;
		NoiseBuffer mbuf;
		int err, ok;
		size_t message_size;

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

	bool handshakeComplete;
	std::mutex handshakeCompleteMutex;
	void setHandshakeComplete(bool complete) {
		std::lock_guard<std::mutex> hcm(handshakeCompleteMutex);
		handshakeComplete = complete;
	}
	bool getHandshakeComplete() {
		std::lock_guard<std::mutex> hcm(handshakeCompleteMutex);
		return handshakeComplete;
	}

	void recievePacket(sf::Packet packet) {
		log->printf(LOG_LEVEL_INFO, "CLIENT recieving packet");

		//handshakeMutex.lock();
		int action = noise_handshakestate_get_action(handshake);
		//handshakeMutex.unlock();
		NoiseBuffer mbuf;
		int err, ok;
		size_t message_size;

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

//	int execHandshake() {
//		NoiseHandshakeState* handshake;
//		NoiseCipherState* send_cipher = 0;
//		NoiseCipherState* recv_cipher = 0;
//		NoiseRandState* rand = 0;
//		NoiseBuffer mbuf;
//		EchoProtocolId id;
//		int err, ok;
//		int action;
//		//int fd;
//		size_t message_size;
//		size_t max_line_len;
//
//		ok = 1;
//
//		if (noise_init() != NOISE_ERROR_NONE) {
//			fprintf(stderr, "Noise initialization failed\n");
//			return 1;
//		}
//
//		/* Check that the echo protocol supports the handshake protocol.
//		   One-way handshake patterns and XXfallback are not yet supported. */
//		std::string protocol = "Noise_NN_25519_AESGCM_SHA256";
//		if (!echo_get_protocol_id(&id, protocol.c_str())) {
//			fprintf(stderr, "%s: not supported by the echo protocol\n", protocol.c_str());
//			return 1;
//		}
//
//		/* Create a HandshakeState object for the protocol */
//		err = noise_handshakestate_new_by_name
//			(&handshake, protocol.c_str(), NOISE_ROLE_INITIATOR);
//		if (err != NOISE_ERROR_NONE) {
//			noise_perror(protocol.c_str(), err);
//			return 1;
//		}
//
//		/* Set the handshake options and verify that everything we need
//		   has been supplied on the command-line. */
//		if (!initialize_handshake(handshake, &id, sizeof(id))) {
//			noise_handshakestate_free(handshake);
//			return 1;
//		}
//
//		log->printf(LOG_LEVEL_INFO, "NOISE: attempting to connect to server......");
//		log->printf(LOG_LEVEL_INFO, "attempting to connect to server, " + ipAddress + ":" + std::to_string(port));
//		sf::IpAddress myaddr(ipAddress);
//		server->connect(myaddr, port);
//		log->printf(LOG_LEVEL_INFO, "connected to server");
//
//		sf::Packet idPacket;
//		idPacket.append(&id, sizeof(id));
//		log->printf(LOG_LEVEL_INFO, "CLIENT appended " + std::to_string(sizeof(id)) + " bytes to packet for id");
//		server->send(idPacket); // this definitely isn't going to work
//
//		err = noise_handshakestate_start(handshake);
//		if (err != NOISE_ERROR_NONE) {
//			log->printf(LOG_LEVEL_INFO, "CLIENT start handshake failed");
//			noise_perror("start handshake", err);
//			ok = 0;
//		}
//
//		/* Run the handshake until we run out of things to read or write */
//		while (ok) {
//			action = noise_handshakestate_get_action(handshake);
//			if (action == NOISE_ACTION_WRITE_MESSAGE) {
//				/* Write the next handshake message with a zero-length payload */
//				noise_buffer_set_output(mbuf, message + 2, sizeof(message) - 2);
//				err = noise_handshakestate_write_message(handshake, &mbuf, NULL);
//				if (err != NOISE_ERROR_NONE) {
//					noise_perror("write handshake", err);
//					ok = 0;
//					break;
//				}
//				message[0] = (uint8_t)(mbuf.size >> 8);
//				message[1] = (uint8_t)mbuf.size;
//				/*if (!echo_send(fd, message, mbuf.size + 2)) {
//					ok = 0;
//					break;
//				}*/
//				sf::Packet packet;
//				packet.append(message, mbuf.size + 2);
//				server->send(packet);
//			} else if (action == NOISE_ACTION_READ_MESSAGE) {
//				/* Read the next handshake message and discard the payload */
//				sf::Packet packet;
//				server->receive(packet);
//				message_size = packet.getDataSize();
//				memcpy(message, packet.getData(), packet.getDataSize());
//				/*message_size = echo_recv(fd, message, sizeof(message));
//				if (!message_size) {
//					ok = 0;
//					break;
//				}*/
//
//				noise_buffer_set_input(mbuf, message + 2, message_size - 2);
//				err = noise_handshakestate_read_message(handshake, &mbuf, NULL);
//				if (err != NOISE_ERROR_NONE) {
//					noise_perror("read handshake", err);
//					ok = 0;
//					break;
//				}
//			} else {
//				/* Either the handshake has finished or it has failed */
//				break;
//			}
//		}
//
//		return 1;
//	}

	/* Convert a Noise handshake protocol name into an Echo protocol id */
	int echo_get_protocol_id(EchoProtocolId *id, const char *name)
	{
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

	/* Initialize's the handshake using command-line options */
	int initialize_handshake
	    (NoiseHandshakeState *handshake, const void *prologue, size_t prologue_len)
	{
	    NoiseDHState *dh;
	    uint8_t *key = 0;
	    size_t key_len = 0;
	    int err;

	    /* Set the prologue first */
	    err = noise_handshakestate_set_prologue(handshake, prologue, prologue_len);
	    if (err != NOISE_ERROR_NONE) {
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
//	    if (noise_handshakestate_needs_local_keypair(handshake)) {
//	        if (client_private_key) {
//	            dh = noise_handshakestate_get_local_keypair_dh(handshake);
//	            key_len = noise_dhstate_get_private_key_length(dh);
//	            key = (uint8_t *)malloc(key_len);
//	            if (!key)
//	                return 0;
//	            if (!echo_load_private_key(client_private_key.c_str(), key, key_len)) {
//	                noise_free(key, key_len);
//	                return 0;
//	            }
//	            err = noise_dhstate_set_keypair_private(dh, key, key_len);
//	            noise_free(key, key_len);
//	            if (err != NOISE_ERROR_NONE) {
//	                noise_perror("set client private key", err);
//	                return 0;
//	            }
//	        } else {
//	            fprintf(stderr, "Client private key required, but not provided.\n");
//	            return 0;
//	        }
//	    }
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
	    return 1;
	}

	sf::TcpSocket* server;
	CryptoKernel::Log* log;
	std::string ipAddress;
	uint64_t port;

	std::string client_public_key;
	std::string server_public_key;
	std::string client_private_key;
	std::string psk;
	std::string psk_file;

	std::mutex handshakeMutex;

	NoiseHandshakeState* handshake;
	EchoProtocolId id;
	std::unique_ptr<std::thread> writeInfoThread;
	bool sentId;

	NoiseCipherState* send_cipher;
	NoiseCipherState* recv_cipher;
};

class NoiseConnectionServer {
public:
	/* Loads a binary private key from a file.  Returns non-zero if OK. */
	int echo_load_private_key(const char *filename, uint8_t *key, size_t len)
	{
		FILE *file = fopen(filename, "rb");
		size_t posn = 0;
		int ch;
		if (len > MAX_DH_KEY_LEN) {
			fprintf(stderr, "private key length is not supported\n");
			return 0;
		}
		if (!file) {
			perror(filename);
			return 0;
		}
		while ((ch = getc(file)) != EOF) {
			if (posn >= len) {
				fclose(file);
				fprintf(stderr, "%s: private key value is too long\n", filename);
				return 0;
			}
			key[posn++] = (uint8_t)ch;
		}
		if (posn < len) {
			fclose(file);
			fprintf(stderr, "%s: private key value is too short\n", filename);
			return 0;
		}
		fclose(file);
		return 1;
	}

	/* Loads a base64-encoded public key from a file.  Returns non-zero if OK. */
	int echo_load_public_key(const char *filename, uint8_t *key, size_t len)
	{
		FILE *file = fopen(filename, "rb");
		uint32_t group = 0;
		size_t group_size = 0;
		uint32_t digit = 0;
		size_t posn = 0;
		int ch;
		if (len > MAX_DH_KEY_LEN) {
			fprintf(stderr, "public key length is not supported\n");
			return 0;
		}
		if (!file) {
			perror(filename);
			return 0;
		}
		while ((ch = getc(file)) != EOF) {
			if (ch >= 'A' && ch <= 'Z') {
				digit = ch - 'A';
			} else if (ch >= 'a' && ch <= 'z') {
				digit = ch - 'a' + 26;
			} else if (ch >= '0' && ch <= '9') {
				digit = ch - '0' + 52;
			} else if (ch == '+') {
				digit = 62;
			} else if (ch == '/') {
				digit = 63;
			} else if (ch == '=') {
				break;
			} else if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
				fclose(file);
				fprintf(stderr, "%s: invalid character in public key file\n", filename);
				return 0;
			}
			group = (group << 6) | digit;
			if (++group_size >= 4) {
				if ((len - posn) < 3) {
					fclose(file);
					fprintf(stderr, "%s: public key value is too long\n", filename);
					return 0;
				}
				group_size = 0;
				key[posn++] = (uint8_t)(group >> 16);
				key[posn++] = (uint8_t)(group >> 8);
				key[posn++] = (uint8_t)group;
			}
		}
		if (group_size == 3) {
			if ((len - posn) < 2) {
				fclose(file);
				fprintf(stderr, "%s: public key value is too long\n", filename);
				return 0;
			}
			key[posn++] = (uint8_t)(group >> 10);
			key[posn++] = (uint8_t)(group >> 2);
		} else if (group_size == 2) {
			if ((len - posn) < 1) {
				fclose(file);
				fprintf(stderr, "%s: public key value is too long\n", filename);
				return 0;
			}
			key[posn++] = (uint8_t)(group >> 4);
		}
		if (posn < len) {
			fclose(file);
			fprintf(stderr, "%s: public key value is too short\n", filename);
			return 0;
		}
		fclose(file);
		return 1;
	}

	NoiseConnectionServer(sf::TcpSocket* client, uint64_t port, CryptoKernel::Log* log) {
		send_cipher = 0;
		recv_cipher = 0;

		this->client = client;
		this->log = log;
		this->port = port;

		handshake = 0;
		message_size = 0;
		recievedId = false;

		handshakeComplete = false;

		if (noise_init() != NOISE_ERROR_NONE) {
			fprintf(stderr, "Noise initialization failed\n");
			//return 1;
		}

		if (!echo_load_private_key
				("server_key_25519", server_key_25519, sizeof(server_key_25519))) {
			log->printf(LOG_LEVEL_INFO, "could not load server key 25519");
			//return 1;
		}
		if (!echo_load_private_key
				("server_key_448", server_key_448, sizeof(server_key_448))) {
			log->printf(LOG_LEVEL_INFO, "could not load server key 448");
			//return 1;
		}
		if (!echo_load_public_key
				("client_key_25519.pub", client_key_25519, sizeof(client_key_25519))) {
			log->printf(LOG_LEVEL_INFO, "could not load client key 25519");
			//return 1;
		}
		if (!echo_load_public_key
				("client_key_448.pub", client_key_448, sizeof(client_key_448))) {
			log->printf(LOG_LEVEL_INFO, "could not load client key 448");
			//return 1;
		}
		if (!echo_load_public_key("psk", psk, sizeof(psk))) {
			log->printf(LOG_LEVEL_INFO, "could not load key psk");
			//return 1;
		}

		writeInfoThread.reset(new std::thread(&NoiseConnectionServer::writeInfo, this)); // start the write info thread
	}

	~NoiseConnectionServer() {
		writeInfoThread->join();
	}

	void writeInfo() {
		log->printf(LOG_LEVEL_INFO, "SERVER write info starting");
		bool yippy = false;

		while(true) {
			//handshakeMutex.lock();
			int action = noise_handshakestate_get_action(handshake);
			//handshakeMutex.unlock();
			if (action == NOISE_ACTION_WRITE_MESSAGE) {
				log->printf(LOG_LEVEL_INFO, "hehe, SERVER looks like we're actually gonna write something");
				/* Write the next handshake message with a zero-length payload */
				noise_buffer_set_output(mbuf, message + 2, sizeof(message) - 2);
				int err = noise_handshakestate_write_message(handshake, &mbuf, NULL);
				if (err != NOISE_ERROR_NONE) {
					noise_perror("write handshake error!! ", err);
					//ok = 0;
					//break;
				}
				message[0] = (uint8_t)(mbuf.size >> 8);
				message[1] = (uint8_t)mbuf.size;
				/*if (!echo_send(fd, message, mbuf.size + 2)) {
					ok = 0;
					break;
				}*/

				sf::Packet packet;
				packet.append(message, mbuf.size + 2);
				client->send(packet);

				if(yippy) {
					log->printf(LOG_LEVEL_INFO, "yippy happened, but we got back to this state??");
				}
			}
			else if(action != NOISE_ACTION_READ_MESSAGE && action != NOISE_ACTION_NONE) { // for some reason, a noise action none fires
				yippy = true;
				log->printf(LOG_LEVEL_INFO, "SERVER yippy " + std::to_string(action));
				break;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(50)); // totally arbitrary
		}

		/* If the action is not "split", then the handshake has failed */
		int ok = 1;
		if (ok && noise_handshakestate_get_action(handshake) != NOISE_ACTION_SPLIT) {
			log->printf(LOG_LEVEL_INFO, "SERVER handshake failed");
			fprintf(stderr, "protocol handshake failed\n");
			ok = 0;
		}

		/* Split out the two CipherState objects for send and receive */
		int err;
		if (ok) {
			err = noise_handshakestate_split(handshake, &send_cipher, &recv_cipher);
			if (err != NOISE_ERROR_NONE) {
				noise_perror("split to start data transfer", err);
				ok = 0;
			}
		}

		/* We no longer need the HandshakeState */
		noise_handshakestate_free(handshake);
		handshake = 0;

		log->printf(LOG_LEVEL_INFO, "SERVER handshake succeeded!");

		setHandshakeComplete(true);
		//handshakeSuccessful = true;
	}

	bool handshakeComplete;
	std::mutex handshakeCompleteMutex;
	void setHandshakeComplete(bool complete) {
		std::lock_guard<std::mutex> hcm(handshakeCompleteMutex);
		handshakeComplete = complete;
	}
	bool getHandshakeComplete() {
		std::lock_guard<std::mutex> hcm(handshakeCompleteMutex);
		return handshakeComplete;
	}

	void recievePacket(sf::Packet packet) {
		int err;
		log->printf(LOG_LEVEL_INFO, "SERVER HAHAHAHA RECIEVE PACKET STARTING");

		if(!recievedId) {
			//sf::Packet idBytes;
			/*if(client->receive(idBytes) != sf::Socket::Done) {
				log->printf(LOG_LEVEL_INFO, "Did not receive the echo protocol identifier");
				fprintf(stderr, "Did not receive the echo protocol identifier\n");
				//ok = 0;
			}*/

			log->printf(LOG_LEVEL_INFO, "Server says the id pattern size is " + std::to_string(packet.getDataSize()));
			//void* cool = &id;
			memcpy(&id, packet.getData(), (unsigned long int)packet.getDataSize());

			log->printf(LOG_LEVEL_INFO, "Server says the id pattern itself is " + std::to_string(id.pattern));
			recievedId = true;

			/* Convert the echo protocol identifier into a Noise protocol identifier */
			bool ok = true;
			NoiseProtocolId nid;
			if (ok && !echo_to_noise_protocol_id(&nid, &id)) {
				log->printf(LOG_LEVEL_INFO, "Unknown echo protocol identifier");
				fprintf(stderr, "Unknown echo protocol identifier\n");
				ok = 0;
			}

			/* Create a HandshakeState object to manage the server's handshake */
			if (ok) {
				err = noise_handshakestate_new_by_id
					(&handshake, &nid, NOISE_ROLE_RESPONDER);
				if (err != NOISE_ERROR_NONE) {
					log->printf(LOG_LEVEL_INFO, "well, couldn't create the handshake");
					noise_perror("create handshake", err);
					ok = 0;
				}
			}

			/* Set all keys that are needed by the client's requested echo protocol */
			if (ok) {
				if (!initialize_handshake(handshake, &nid, &id, sizeof(id))) {
					log->printf(LOG_LEVEL_INFO, "couldn't initialize handshake");
					ok = 0;
				}
			}

			/* Start the handshake */
			if (ok) {
				err = noise_handshakestate_start(handshake);
				if (err != NOISE_ERROR_NONE) {
					log->printf(LOG_LEVEL_INFO, "couldn't start handshake");
					noise_perror("start handshake", err);
					ok = 0;
				}
			}
		}
		else {
			//std::lock_guard<std::mutex> hm(handshakeMutex);
			int action = noise_handshakestate_get_action(handshake);
			if(action == NOISE_ACTION_READ_MESSAGE) {
				log->printf(LOG_LEVEL_INFO, "SERVER READING MESSAGE!!!");
				/* Read the next handshake message and discard the payload */
				/*message_size = echo_recv(fd, message, sizeof(message));
				if (!message_size) {
					ok = 0;
					break;
				}*/

				//sf::Packet packet;
				//client->receive(packet);
				message_size = packet.getDataSize();
				memcpy(message, packet.getData(), packet.getDataSize());

				noise_buffer_set_input(mbuf, message + 2, message_size - 2);
				err = noise_handshakestate_read_message(handshake, &mbuf, NULL);
				if (err != NOISE_ERROR_NONE) {
					log->printf(LOG_LEVEL_INFO, "SERVER READ MESSAGE FAIL");
					noise_perror("read handshake", err);
					//ok = 0;
					//break;
				}
			}
		}
	}

//	int execHandshake() {
//		NoiseHandshakeState *handshake = 0;
//		NoiseCipherState *send_cipher = 0;
//		NoiseCipherState *recv_cipher = 0;
//		EchoProtocolId id;
//		NoiseProtocolId nid;
//		NoiseBuffer mbuf;
//		size_t message_size;
//		int fd;
//		int err;
//		int ok = 1;
//		int action;
//
//		if (noise_init() != NOISE_ERROR_NONE) {
//			//fprintf(stderr, "Noise initialization failed\n");
//			return 1;
//		}
//
//		if (!echo_load_private_key
//				("server_key_25519", server_key_25519, sizeof(server_key_25519))) {
//			log->printf(LOG_LEVEL_INFO, "could not load server key 25519");
//			return 1;
//		}
//		if (!echo_load_private_key
//				("server_key_448", server_key_448, sizeof(server_key_448))) {
//			log->printf(LOG_LEVEL_INFO, "could not load server key 448");
//			return 1;
//		}
//		if (!echo_load_public_key
//				("client_key_25519.pub", client_key_25519, sizeof(client_key_25519))) {
//			log->printf(LOG_LEVEL_INFO, "could not load client key 25519");
//			return 1;
//		}
//		if (!echo_load_public_key
//				("client_key_448.pub", client_key_448, sizeof(client_key_448))) {
//			log->printf(LOG_LEVEL_INFO, "could not load client key 448");
//			return 1;
//		}
//		if (!echo_load_public_key("psk", psk, sizeof(psk))) {
//			log->printf(LOG_LEVEL_INFO, "could not load key psk");
//			return 1;
//		}
//
//		log->printf(LOG_LEVEL_INFO, "waiting for client to connect");
//		sf::TcpListener ls;
//		ls.listen(port);
//		if(ls.accept(*client) == sf::Socket::Done) {
//			log->printf(LOG_LEVEL_INFO, "client connected");
//		}
//
//		sf::Packet idBytes;
//		if(client->receive(idBytes) != sf::Socket::Done) {
//			log->printf(LOG_LEVEL_INFO, "Did not receive the echo protocol identifier");
//			fprintf(stderr, "Did not receive the echo protocol identifier\n");
//			ok = 0;
//		}
//
//		//idBytes >> (char*)&id; // no way this works
//		/*std::string idBytesString;
//		idBytes >> idBytesString;
//		log->printf(LOG_LEVEL_INFO, idBytesString);
//		log->printf(LOG_LEVEL_INFO, std::to_string(idBytesString.length()));
//
//		log->printf(LOG_LEVEL_INFO, "Server says the id pattern from the client is " + id.pattern);*/
//
//		/* Read the echo protocol identifier sent by the client */
//		/*if (ok && !echo_recv_exact(fd, (uint8_t *)&id, sizeof(id))) {
//			fprintf(stderr, "Did not receive the echo protocol identifier\n");
//			ok = 0;
//		}*/
//		log->printf(LOG_LEVEL_INFO, "Server says the id pattern size is " + std::to_string(idBytes.getDataSize()));
//		//void* cool = &id;
//		memcpy(&id, idBytes.getData(), (unsigned long int)idBytes.getDataSize());
//
//		log->printf(LOG_LEVEL_INFO, "Server says the id pattern itself is " + std::to_string(id.pattern));
//
//		/* Convert the echo protocol identifier into a Noise protocol identifier */
//		if (ok && !echo_to_noise_protocol_id(&nid, &id)) {
//			log->printf(LOG_LEVEL_INFO, "Unknown echo protocol identifier");
//			fprintf(stderr, "Unknown echo protocol identifier\n");
//			ok = 0;
//		}
//
//		/* Create a HandshakeState object to manage the server's handshake */
//		if (ok) {
//			err = noise_handshakestate_new_by_id
//				(&handshake, &nid, NOISE_ROLE_RESPONDER);
//			if (err != NOISE_ERROR_NONE) {
//				log->printf(LOG_LEVEL_INFO, "well, couldn't create the handshake");
//				noise_perror("create handshake", err);
//				ok = 0;
//			}
//		}
//
//		/* Set all keys that are needed by the client's requested echo protocol */
//		if (ok) {
//			if (!initialize_handshake(handshake, &nid, &id, sizeof(id))) {
//				log->printf(LOG_LEVEL_INFO, "couldn't initialize handshake");
//				ok = 0;
//			}
//		}
//
//		/* Start the handshake */
//		if (ok) {
//			err = noise_handshakestate_start(handshake);
//			if (err != NOISE_ERROR_NONE) {
//				log->printf(LOG_LEVEL_INFO, "couldn't start handshake");
//				noise_perror("start handshake", err);
//				ok = 0;
//			}
//		}
//
//		/* Run the handshake until we run out of things to read or write */
//		while (ok) {
//			action = noise_handshakestate_get_action(handshake);
//			if (action == NOISE_ACTION_WRITE_MESSAGE) {
//				/* Write the next handshake message with a zero-length payload */
//				noise_buffer_set_output(mbuf, message + 2, sizeof(message) - 2);
//				err = noise_handshakestate_write_message(handshake, &mbuf, NULL);
//				if (err != NOISE_ERROR_NONE) {
//					noise_perror("write handshake", err);
//					ok = 0;
//					break;
//				}
//				message[0] = (uint8_t)(mbuf.size >> 8);
//				message[1] = (uint8_t)mbuf.size;
//				/*if (!echo_send(fd, message, mbuf.size + 2)) {
//					ok = 0;
//					break;
//				}*/
//
//				sf::Packet packet;
//				packet.append(message, mbuf.size + 2);
//				client->send(packet);
//			} else if (action == NOISE_ACTION_READ_MESSAGE) {
//				/* Read the next handshake message and discard the payload */
//				/*message_size = echo_recv(fd, message, sizeof(message));
//				if (!message_size) {
//					ok = 0;
//					break;
//				}*/
//				sf::Packet packet;
//				client->receive(packet);
//				message_size = packet.getDataSize();
//				memcpy(message, packet.getData(), packet.getDataSize());
//
//				noise_buffer_set_input(mbuf, message + 2, message_size - 2);
//				err = noise_handshakestate_read_message(handshake, &mbuf, NULL);
//				if (err != NOISE_ERROR_NONE) {
//					noise_perror("read handshake", err);
//					ok = 0;
//					break;
//				}
//			} else {
//				/* Either the handshake has finished or it has failed */
//				break;
//			}
//		}
//
//		/* If the action is not "split", then the handshake has failed */
//		if (ok && noise_handshakestate_get_action(handshake) != NOISE_ACTION_SPLIT) {
//			log->printf(LOG_LEVEL_INFO, "eh, the handshake failed");
//			fprintf(stderr, "protocol handshake failed\n");
//			ok = 0;
//		}
//
//		return 1;
//	}

	/* Initialize's the handshake with all necessary keys */
	int initialize_handshake(NoiseHandshakeState *handshake, const NoiseProtocolId *nid, const void *prologue, size_t prologue_len)
	{
	    NoiseDHState *dh;
	    int dh_id;
	    int err;

	    /* Set the prologue first */
	    err = noise_handshakestate_set_prologue(handshake, prologue, prologue_len);
	    if (err != NOISE_ERROR_NONE) {
	        noise_perror("prologue", err);
	        return 0;
	    }

	    /* Set the PSK if one is needed */
	    if (nid->prefix_id == NOISE_PREFIX_PSK) {
	        err = noise_handshakestate_set_pre_shared_key
	            (handshake, psk, sizeof(psk));
	        if (err != NOISE_ERROR_NONE) {
	            noise_perror("psk", err);
	            return 0;
	        }
	    }

	    /* Set the local keypair for the server based on the DH algorithm */
	    if (noise_handshakestate_needs_local_keypair(handshake)) {
	        dh = noise_handshakestate_get_local_keypair_dh(handshake);
	        dh_id = noise_dhstate_get_dh_id(dh);
	        if (dh_id == NOISE_DH_CURVE25519) {
	            err = noise_dhstate_set_keypair_private
	                (dh, server_key_25519, sizeof(server_key_25519));
	        } else if (dh_id == NOISE_DH_CURVE448) {
	            err = noise_dhstate_set_keypair_private
	                (dh, server_key_448, sizeof(server_key_448));
	        } else {
	            err = NOISE_ERROR_UNKNOWN_ID;
	        }
	        if (err != NOISE_ERROR_NONE) {
	            noise_perror("set server private key", err);
	            return 0;
	        }
	    }

	    /* Set the remote public key for the client */
	    if (noise_handshakestate_needs_remote_public_key(handshake)) {
	        dh = noise_handshakestate_get_remote_public_key_dh(handshake);
	        dh_id = noise_dhstate_get_dh_id(dh);
	        if (dh_id == NOISE_DH_CURVE25519) {
	            err = noise_dhstate_set_public_key
	                (dh, client_key_25519, sizeof(client_key_25519));
	        } else if (dh_id == NOISE_DH_CURVE448) {
	            err = noise_dhstate_set_public_key
	                (dh, client_key_448, sizeof(client_key_448));
	        } else {
	            err = NOISE_ERROR_UNKNOWN_ID;
	        }
	        if (err != NOISE_ERROR_NONE) {
	            noise_perror("set client public key", err);
	            return 0;
	        }
	    }

	    /* Ready to go */
	    return 1;
	}

	/* Convert an Echo protocol id into a Noise protocol id */
	int echo_to_noise_protocol_id(NoiseProtocolId *nid, const EchoProtocolId *id)
	{
	    int ok = 1;

	    memset(nid, 0, sizeof(NoiseProtocolId));

	    switch (id->psk) {
	    case ECHO_PSK_DISABLED:         nid->prefix_id = NOISE_PREFIX_STANDARD; break;
	    case ECHO_PSK_ENABLED:          nid->prefix_id = NOISE_PREFIX_PSK; break;
	    default:                        ok = 0;
	    }

	    switch (id->pattern) {
	    case ECHO_PATTERN_NN:           nid->pattern_id = NOISE_PATTERN_NN; break;
	    case ECHO_PATTERN_KN:           nid->pattern_id = NOISE_PATTERN_KN; break;
	    case ECHO_PATTERN_NK:           nid->pattern_id = NOISE_PATTERN_NK; break;
	    case ECHO_PATTERN_KK:           nid->pattern_id = NOISE_PATTERN_KK; break;
	    case ECHO_PATTERN_NX:           nid->pattern_id = NOISE_PATTERN_NX; break;
	    case ECHO_PATTERN_KX:           nid->pattern_id = NOISE_PATTERN_KX; break;
	    case ECHO_PATTERN_XN:           nid->pattern_id = NOISE_PATTERN_XN; break;
	    case ECHO_PATTERN_IN:           nid->pattern_id = NOISE_PATTERN_IN; break;
	    case ECHO_PATTERN_XK:           nid->pattern_id = NOISE_PATTERN_XK; break;
	    case ECHO_PATTERN_IK:           nid->pattern_id = NOISE_PATTERN_IK; break;
	    case ECHO_PATTERN_XX:           nid->pattern_id = NOISE_PATTERN_XX; break;
	    case ECHO_PATTERN_IX:           nid->pattern_id = NOISE_PATTERN_IX; break;
	    case ECHO_PATTERN_NN | ECHO_PATTERN_HFS:    nid->pattern_id = NOISE_PATTERN_NN_HFS; break;
	    case ECHO_PATTERN_KN | ECHO_PATTERN_HFS:    nid->pattern_id = NOISE_PATTERN_KN_HFS; break;
	    case ECHO_PATTERN_NK | ECHO_PATTERN_HFS:    nid->pattern_id = NOISE_PATTERN_NK_HFS; break;
	    case ECHO_PATTERN_KK | ECHO_PATTERN_HFS:    nid->pattern_id = NOISE_PATTERN_KK_HFS; break;
	    case ECHO_PATTERN_NX | ECHO_PATTERN_HFS:    nid->pattern_id = NOISE_PATTERN_NX_HFS; break;
	    case ECHO_PATTERN_KX | ECHO_PATTERN_HFS:    nid->pattern_id = NOISE_PATTERN_KX_HFS; break;
	    case ECHO_PATTERN_XN | ECHO_PATTERN_HFS:    nid->pattern_id = NOISE_PATTERN_XN_HFS; break;
	    case ECHO_PATTERN_IN | ECHO_PATTERN_HFS:    nid->pattern_id = NOISE_PATTERN_IN_HFS; break;
	    case ECHO_PATTERN_XK | ECHO_PATTERN_HFS:    nid->pattern_id = NOISE_PATTERN_XK_HFS; break;
	    case ECHO_PATTERN_IK | ECHO_PATTERN_HFS:    nid->pattern_id = NOISE_PATTERN_IK_HFS; break;
	    case ECHO_PATTERN_XX | ECHO_PATTERN_HFS:    nid->pattern_id = NOISE_PATTERN_XX_HFS; break;
	    case ECHO_PATTERN_IX | ECHO_PATTERN_HFS:    nid->pattern_id = NOISE_PATTERN_IX_HFS; break;
	    default:                        ok = 0;
	    }

	    switch (id->cipher) {
	    case ECHO_CIPHER_CHACHAPOLY:    nid->cipher_id = NOISE_CIPHER_CHACHAPOLY; break;
	    case ECHO_CIPHER_AESGCM:        nid->cipher_id = NOISE_CIPHER_AESGCM; break;
	    default:                        ok = 0;
	    }

	    switch (id->dh & ECHO_DH_MASK) {
	    case ECHO_DH_25519:             nid->dh_id = NOISE_DH_CURVE25519; break;
	    case ECHO_DH_448:               nid->dh_id = NOISE_DH_CURVE448; break;
	    case ECHO_DH_NEWHOPE:           nid->dh_id = NOISE_DH_NEWHOPE; break;
	    default:                        ok = 0;
	    }

	    switch (id->dh & ECHO_HYBRID_MASK) {
	    case ECHO_HYBRID_25519:         nid->hybrid_id = NOISE_DH_CURVE25519; break;
	    case ECHO_HYBRID_448:           nid->hybrid_id = NOISE_DH_CURVE448; break;
	    case ECHO_HYBRID_NEWHOPE:       nid->hybrid_id = NOISE_DH_NEWHOPE; break;
	    case ECHO_HYBRID_NONE:          nid->hybrid_id = NOISE_DH_NONE; break;
	    default:                        ok = 0;
	    }

	    switch (id->hash) {
	    case ECHO_HASH_SHA256:          nid->hash_id = NOISE_HASH_SHA256; break;
	    case ECHO_HASH_SHA512:          nid->hash_id = NOISE_HASH_SHA512; break;
	    case ECHO_HASH_BLAKE2s:         nid->hash_id = NOISE_HASH_BLAKE2s; break;
	    case ECHO_HASH_BLAKE2b:         nid->hash_id = NOISE_HASH_BLAKE2b; break;
	    default:                        ok = 0;
	    }

	    return ok;
	}

	uint8_t client_key_25519[CURVE25519_KEY_LEN];
	uint8_t server_key_25519[CURVE25519_KEY_LEN];
	uint8_t client_key_448[CURVE448_KEY_LEN];
	uint8_t server_key_448[CURVE448_KEY_LEN];
	uint8_t psk[32];

	sf::TcpSocket* client;
	CryptoKernel::Log* log;
	uint64_t port;

	bool recievedId;
	size_t message_size;
	NoiseBuffer mbuf;
	NoiseHandshakeState *handshake;
	EchoProtocolId id;

	std::mutex handshakeMutex;
	std::unique_ptr<std::thread> writeInfoThread;

	NoiseCipherState* send_cipher;
	NoiseCipherState* recv_cipher;
};


#endif /* NOISECONNECTION_H_ */
