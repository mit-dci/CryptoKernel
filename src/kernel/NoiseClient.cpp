/*
 * Copyright (C) 2016 Southern Storm Software, Pty Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "NoiseClient.h"
#include <unistd.h>
#include <stdlib.h>

NoiseClient::NoiseClient(std::shared_ptr<sf::TcpSocket> server, std::string ipAddress, uint64_t port, CryptoKernel::Log* log) {
	this->server = server;
	this->log = log;
	this->ipAddress = ipAddress;
	this->port = port;

	clientPrivateKey = "keys/client_key_25519";

	send_cipher = 0;
	recv_cipher = 0;

	sentId = false;

	handshakeComplete = false;
	handshakeSuccess = false;

	log->printf(LOG_LEVEL_INFO, "Noise(): Client started");

	if(noise_init() != NOISE_ERROR_NONE) {
		log->printf(LOG_LEVEL_WARN, "Noise initialization failed");
		return;
	}

	std::string protocol = "Noise_XX_25519_AESGCM_SHA256";
	prologue.pattern = PATTERN_XX;
	prologue.psk = PSK_DISABLED;
	prologue.cipher = CIPHER_AESGCM;
	prologue.hash = HASH_SHA256;
	prologue.dh = DH_25519;

	/* Create a HandshakeState object for the protocol */
	int err = noise_handshakestate_new_by_name(&handshake, protocol.c_str(), NOISE_ROLE_INITIATOR);
	if(err != NOISE_ERROR_NONE) {
		log->printf(LOG_LEVEL_WARN, "Client, error: " + noiseUtil.errToString(err));
		return;
	}

	log->printf(LOG_LEVEL_INFO, "Noise(): Client, starting writeInfo.");
	writeInfoThread.reset(new std::thread(&NoiseClient::writeInfo, this)); // start the write info thread
    receiveThread.reset(new std::thread(&NoiseClient::receiveWrapper, this));
}

void NoiseClient::writeInfo() {
	log->printf(LOG_LEVEL_INFO, "Noise(): Client writing info");

	int action;
	NoiseBuffer mbuf;
	int err, ok;
	size_t message_size;

	uint8_t message[MAX_MESSAGE_LEN + 2];

	ok = 1;

	bool sentPubKey = false;

	while(!getHandshakeComplete()) { // it might fail in another thread, and so become "complete"
		if(server->getRemoteAddress() == sf::IpAddress::None) {
			log->printf(LOG_LEVEL_WARN, "Noise(): Client, the remote port has been invalidated");
			setHandshakeComplete(true, false);
			ok = 0;
			continue;
		}

		if(!sentPubKey) { // we need to share our public key over the network
			log->printf(LOG_LEVEL_INFO, "Noise(): Client sending public key to " + server->getRemoteAddress().toString());
			sf::Packet pubKeyPacket;
			if(!noiseUtil.loadPublicKey("keys/client_key_25519.pub", clientKey25519, sizeof(clientKey25519))) {
				uint8_t* privKey = 0;
				uint8_t* pubKey = 0;
				if(!noiseUtil.writeKeys("keys/client_key_25519.pub", "keys/client_key_25519", &pubKey, &privKey)) {
					setHandshakeComplete(true, false);
					ok = 0;
					delete pubKey;
					delete privKey;
					continue;
				}
				memcpy(clientKey25519, pubKey, CURVE25519_KEY_LEN); // put the new key in its proper place
				delete pubKey;
				delete privKey;
			}

			handshakeMutex.lock();
			if(!initializeHandshake(handshake, &prologue, sizeof(prologue))) { // now that we have keys, initialize handshake
				log->printf(LOG_LEVEL_WARN, "Noise(): Client, error initializing handshake.");
				ok = 0;
				handshakeMutex.unlock();
				setHandshakeComplete(true, false);
				continue;
			}
			handshakeMutex.unlock();

			pubKeyPacket.append(clientKey25519, sizeof(clientKey25519));

			if(server->send(pubKeyPacket) != sf::Socket::Done) {
				continue; // keep sending the public key until it goes through
			}

			sentPubKey = true;
			err = noise_handshakestate_start(handshake);
			if(err != NOISE_ERROR_NONE) {
				log->printf(LOG_LEVEL_WARN, "Noise(): Client start handshake error, " + noiseUtil.errToString(err));
				setHandshakeComplete(true, false);
				ok = 0;
				continue;
			}
		}
		else {
			handshakeMutex.lock();
			action = noise_handshakestate_get_action(handshake);
			handshakeMutex.unlock();

			if(action == NOISE_ACTION_WRITE_MESSAGE) {
				/* Write the next handshake message with a zero-length payload */
				noise_buffer_set_output(mbuf, message + 2, sizeof(message) - 2);
				handshakeMutex.lock();
				err = noise_handshakestate_write_message(handshake, &mbuf, NULL);
				handshakeMutex.unlock();
				if(err != NOISE_ERROR_NONE) {
					log->printf(LOG_LEVEL_WARN, "Noise(): Client, handshake failed on write message: " + noiseUtil.errToString(err));
					setHandshakeComplete(true, false);
					ok = 0;
					continue;
				}
				message[0] = (uint8_t)(mbuf.size >> 8);
				message[1] = (uint8_t)mbuf.size;

				log->printf(LOG_LEVEL_INFO, "Noise(): Client sending a packet.");
				sf::Packet packet;
				packet.append(message, mbuf.size + 2);
				if(server->send(packet) != sf::Socket::Done) {
					log->printf(LOG_LEVEL_WARN, "Noise(): Client couldn't send packet.");
					setHandshakeComplete(true, false);
					ok = 0;
					continue;
				}
			}
			else if(action != NOISE_ACTION_READ_MESSAGE) {
				log->printf(LOG_LEVEL_INFO, "Noise(): Client, either the handshake succeeded, or it failed");
				break;
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(50)); // again, totally arbitrary
	}

	/* If the action is not "split", then the handshake has failed */
	handshakeMutex.lock();
	if(ok && noise_handshakestate_get_action(handshake) != NOISE_ACTION_SPLIT) {
		log->printf(LOG_LEVEL_WARN, "Noise(): Client, protocol handshake failed");
		ok = 0;
	}
	handshakeMutex.unlock();

	/* Split out the two CipherState objects for send and receive */
	if(ok) {
		handshakeMutex.lock();
		err = noise_handshakestate_split(handshake, &send_cipher, &recv_cipher);
		handshakeMutex.unlock();
		if(err != NOISE_ERROR_NONE) {
			log->printf(LOG_LEVEL_WARN, "Noise(): Client, split failed: " + noiseUtil.errToString(err));
			ok = 0;
		}
	}

	/* We no longer need the HandshakeState */
	handshakeMutex.lock();
	noise_handshakestate_free(handshake);
	handshakeMutex.unlock();
	handshake = 0;

	// padding would go here, if we used it

	/* Tell the user that the handshake has been successful */
	setHandshakeComplete(true, ok);
}

void NoiseClient::receiveWrapper() {
    log->printf(LOG_LEVEL_INFO, "Noise(): Client, receive wrapper starting.");
    sf::SocketSelector selector;
    selector.add(*server.get());
    bool quitThread = false;

    while(!quitThread && !getHandshakeComplete())
    if(selector.wait(sf::seconds(2))) {
        sf::Packet packet;
        const auto status = server->receive(packet);
        if(status == sf::Socket::Done) {
            receivePacket(packet);
        }
        else {
            log->printf(LOG_LEVEL_INFO, "Noise(): Client encountered error receiving packet");
            quitThread = true;
        }
    }
}

void NoiseClient::receivePacket(sf::Packet& packet) {
	log->printf(LOG_LEVEL_INFO, "Noise(): Client receiving a packet.");

	handshakeMutex.lock();
	int action = noise_handshakestate_get_action(handshake);
	handshakeMutex.unlock();
	NoiseBuffer mbuf;
	int err, ok;
	size_t message_size;

	uint8_t message[MAX_MESSAGE_LEN + 2];

	if(action == NOISE_ACTION_READ_MESSAGE) {
		log->printf(LOG_LEVEL_INFO, "Noise(): Client reading a message.");

		message_size = packet.getDataSize();
		memcpy(message, packet.getData(), packet.getDataSize());

		noise_buffer_set_input(mbuf, message + 2, message_size - 2);
		handshakeMutex.lock();
		err = noise_handshakestate_read_message(handshake, &mbuf, NULL);
		handshakeMutex.unlock();
		if(err != NOISE_ERROR_NONE) {
			log->printf(LOG_LEVEL_WARN, "Noise(): Client, read handshake " + noiseUtil.errToString(err));
			setHandshakeComplete(true, false);
		}
	}
}

void NoiseClient::setHandshakeComplete(bool complete, bool success) {
	if(success) {
		log->printf(LOG_LEVEL_INFO, "Noise(): Client handshake succeeded");
	}
	
	std::lock_guard<std::mutex> hcm(handshakeCompleteMutex);
	handshakeComplete = complete;
	handshakeSuccess = success;
}

bool NoiseClient::getHandshakeComplete() {
	std::lock_guard<std::mutex> hcm(handshakeCompleteMutex);
	return handshakeComplete;
}

bool NoiseClient::getHandshakeSuccess() {
	std::lock_guard<std::mutex> hcm(handshakeCompleteMutex);
	return handshakeSuccess;
}

/* Initialize's the handshake using command-line options */
int NoiseClient::initializeHandshake(NoiseHandshakeState* handshake, const void* prologue, size_t prologue_len) {
	NoiseDHState *dh;
	uint8_t *key = 0;
	size_t keyLen = 0;
	int err;

	/* Set the prologue first */
	err = noise_handshakestate_set_prologue(handshake, prologue, prologue_len);
	if(err != NOISE_ERROR_NONE) {
		log->printf(LOG_LEVEL_INFO, "Noise(): Client prologue: " + noiseUtil.errToString(err));
		return 0;
	}

	// we are not using pre-shared keys, but they would be initialized right here

	/* Set the local keypair for the client */
	if(noise_handshakestate_needs_local_keypair(handshake)) {
		if(clientPrivateKey.length() > 0) {
			dh = noise_handshakestate_get_local_keypair_dh(handshake);
			keyLen = noise_dhstate_get_private_key_length(dh);
			key = (uint8_t*)malloc(keyLen);
			if(!key) {
				log->printf(LOG_LEVEL_INFO, "Noise(): Client, could not allocate space for key.");
				return 0;
			}
			if(!noiseUtil.loadPrivateKey(clientPrivateKey.c_str(), key, keyLen)) {
				log->printf(LOG_LEVEL_WARN, "Noise(): Client, could not load private key.");
				noise_free(key, keyLen);
				return 0;
			}
			err = noise_dhstate_set_keypair_private(dh, key, keyLen);
			noise_free(key, keyLen);
			if(err != NOISE_ERROR_NONE) {
				log->printf(LOG_LEVEL_WARN, "Noise(): Set client private key " + noiseUtil.errToString(err));
				return 0;
			}
		}
		else {
			log->printf(LOG_LEVEL_WARN, "Noise(): Client private key required but not provided.");
			return 0;
		}
	}

	// we are not using a remote public key for the server, but it would be initialized right here

	/* Ready to go */
	log->printf(LOG_LEVEL_INFO, "Noise(): Client handshake initialization successful!");
	return 1;
}

NoiseClient::~NoiseClient() {
	log->printf(LOG_LEVEL_INFO, "Cleaning up noise client");
	if(!getHandshakeComplete()) {
		setHandshakeComplete(true, false);
	}
	server->disconnect();
	writeInfoThread->join();
	receiveThread->join();
	log->printf(LOG_LEVEL_INFO, "Cleaned up noise client");
}
