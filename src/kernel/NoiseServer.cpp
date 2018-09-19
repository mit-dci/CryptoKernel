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

#include "NoiseServer.h"

NoiseServer::NoiseServer(sf::TcpSocket* client, uint64_t port, CryptoKernel::Log* log) {
	sendCipher = 0;
	recvCipher = 0;

	this->client = client;
	this->log = log;
	this->port = port;

	handshake = 0;
	messageSize = 0;
	receivedId = false;
	receivedPubKey = false;

	handshakeComplete = false;
	handshakeSuccess = false;

	if(noise_init() != NOISE_ERROR_NONE) {
		log->printf(LOG_LEVEL_WARN, "Noise(): Server, noise initialization failed.");
		return;
	}

	if(!noiseUtil.loadPrivateKey("keys/server_key_25519", serverKey25519, sizeof(serverKey25519))) {
		log->printf(LOG_LEVEL_INFO, "could not load server key 25519");
		uint8_t* serverPubKey;
		uint8_t* serverPrivKey;
		if(!noiseUtil.writeKeys("keys/server_key_25519.pub", "keys/server_key_25519", &serverPubKey, &serverPrivKey)) {
			log->printf(LOG_LEVEL_INFO, "Could not write server keys.");
			setHandshakeComplete(true, false);
			return;
		}
		memcpy(serverKey25519, serverPrivKey, CURVE25519_KEY_LEN); // put the new key in its proper place
		delete serverPubKey;
		delete serverPrivKey;
	}

	prologue.pattern = PATTERN_XX;
	prologue.psk = PSK_DISABLED;
	prologue.cipher = CIPHER_AESGCM;
	prologue.hash = HASH_SHA256;
	prologue.dh = DH_25519;

	nid = (NoiseProtocolId*)malloc(sizeof(NoiseProtocolId));

	nid->prefix_id = NOISE_PREFIX_STANDARD;
	nid->pattern_id = NOISE_PATTERN_XX;
	nid->cipher_id = NOISE_CIPHER_AESGCM;
	nid->dh_id = NOISE_DH_CURVE25519;
	nid->hash_id = NOISE_HASH_SHA256;

	writeInfoThread.reset(new std::thread(&NoiseServer::writeInfo, this)); // start the write info thread
    receiveThread.reset(new std::thread(&NoiseServer::receiveWrapper, this));
}

void NoiseServer::writeInfo() {
	log->printf(LOG_LEVEL_INFO, "SERVER write info starting");

	uint8_t message[MAX_MESSAGE_LEN + 2];
	unsigned int ok = 1;

	while(!getHandshakeComplete()) { // it might fail in another thread, and so become "complete"
		if(!receivedPubKey) {
			continue;
		}
		
		handshakeMutex.lock();
		int action = noise_handshakestate_get_action(handshake);
		handshakeMutex.unlock();
		if(action == NOISE_ACTION_WRITE_MESSAGE) {
			log->printf(LOG_LEVEL_INFO, "Noise(): Server writing message.");
			/* Write the next handshake message with a zero-length payload */
			noise_buffer_set_output(mbuf, message + 2, sizeof(message) - 2);
			handshakeMutex.lock();
			int err = noise_handshakestate_write_message(handshake, &mbuf, NULL);
			handshakeMutex.unlock();
			if (err != NOISE_ERROR_NONE) {
				log->printf(LOG_LEVEL_WARN, "Noise(): Server, error writing message: " + noiseUtil.errToString(err));
				setHandshakeComplete(true, false);
				ok = 0;
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
		else if(action != NOISE_ACTION_READ_MESSAGE && action != NOISE_ACTION_NONE) {
			break;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(50)); // totally arbitrary
	}

	/* If the action is not "split", then the handshake has failed */
	handshakeMutex.lock();
	if(ok && noise_handshakestate_get_action(handshake) != NOISE_ACTION_SPLIT) {
		log->printf(LOG_LEVEL_INFO, "Noise(): Server, handshake failed.");
		ok = 0;
	}
	handshakeMutex.unlock();

	/* Split out the two CipherState objects for send and receive */
	int err;
	if(ok) {
		handshakeMutex.lock();
		err = noise_handshakestate_split(handshake, &sendCipher, &recvCipher);
		if (err != NOISE_ERROR_NONE) {
			log->printf(LOG_LEVEL_WARN, "Noise(): Server, split failed: " + noiseUtil.errToString(err));
			ok = 0;
		}
		handshakeMutex.unlock();
	}

	/* We no longer need the HandshakeState */
	noise_handshakestate_free(handshake);
	handshake = 0;

	setHandshakeComplete(true, ok);
}

void NoiseServer::receiveWrapper() {
    log->printf(LOG_LEVEL_INFO, "Noise(): Server, receive wrapper starting.");
    sf::SocketSelector selector;
    selector.add(*client);
    bool quitThread = false;

    while(!quitThread && !getHandshakeComplete())
    if(selector.wait(sf::seconds(2))) {
        sf::Packet packet;
        const auto status = client->receive(packet);
        if(status == sf::Socket::Done) {
            receivePacket(packet);
        }
        else {
            log->printf(LOG_LEVEL_INFO, "Noise(): Server encountered error receiving packet");
            quitThread = true;
        }
    }

	selector.remove(*client);
}

void NoiseServer::receivePacket(sf::Packet packet) {
	int err;
	log->printf(LOG_LEVEL_INFO, "Noise(): Server, receiving a packet.");
	uint8_t message[MAX_MESSAGE_LEN + 2];

	if(!receivedPubKey) {
		receivedPubKey = true;
		log->printf(LOG_LEVEL_INFO, "received a packet, hopefully the public key!!");
		memcpy(clientKey25519, packet.getData(), packet.getDataSize());

		/* Convert the echo protocol identifier into a Noise protocol identifier */
		bool ok = true;

		/* Create a HandshakeState object to manage the server's handshake */
		if(ok) {
			std::lock_guard<std::mutex> hc(handshakeMutex);
			err = noise_handshakestate_new_by_id
				(&handshake, nid, NOISE_ROLE_RESPONDER);
			if(err != NOISE_ERROR_NONE) {
				log->printf(LOG_LEVEL_INFO, "Noise(): Server, create handshake: " + noiseUtil.errToString(err));
				setHandshakeComplete(true, false);
				return;
			}
		}

		/* Set all keys that are needed by the client's requested echo protocol */
		if(ok) {
			std::lock_guard<std::mutex> hm(handshakeMutex);
			if (!initializeHandshake(handshake, nid, &prologue, sizeof(prologue))) {
				log->printf(LOG_LEVEL_WARN, "Noise(): Server, couldn't initialize handshake");
				setHandshakeComplete(true, false);
				return;
			}
		}

		/* Start the handshake */
		if(ok) {
			std::lock_guard<std::mutex> hc(handshakeMutex);
			err = noise_handshakestate_start(handshake);
			if (err != NOISE_ERROR_NONE) {
				log->printf(LOG_LEVEL_INFO, "Noise(): Server, couldn't start handshake: " + noiseUtil.errToString(err));
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
			messageSize = packet.getDataSize();
			memcpy(message, packet.getData(), packet.getDataSize());

			noise_buffer_set_input(mbuf, message + 2, messageSize - 2);
			err = noise_handshakestate_read_message(handshake, &mbuf, NULL);
			if (err != NOISE_ERROR_NONE) {
				log->printf(LOG_LEVEL_WARN, "Noise(): Server, read handshake error: " + noiseUtil.errToString(err));
				setHandshakeComplete(true, false);
				return;
			}
		}
	}
}

/* Initialize's the handshake with all necessary keys */
int NoiseServer::initializeHandshake(NoiseHandshakeState* handshake, const NoiseProtocolId* nid, const void* prologue, size_t prologueLen) {
	NoiseDHState *dh;
	int dhId;
	int err;

	/* Set the prologue first */
	err = noise_handshakestate_set_prologue(handshake, prologue, prologueLen);
	if(err != NOISE_ERROR_NONE) {
		log->printf(LOG_LEVEL_WARN, "Noise(): Server, prologue error: " + noiseUtil.errToString(err));
		return 0;
	}

	/* Set the local keypair for the server based on the DH algorithm */
	if(noise_handshakestate_needs_local_keypair(handshake)) {
		dh = noise_handshakestate_get_local_keypair_dh(handshake);
		dhId = noise_dhstate_get_dh_id(dh);
		if(dhId == NOISE_DH_CURVE25519) {
			err = noise_dhstate_set_keypair_private(dh, serverKey25519, sizeof(serverKey25519));
		}
		else {
			err = NOISE_ERROR_UNKNOWN_ID;
		}
		if(err != NOISE_ERROR_NONE) {
			log->printf(LOG_LEVEL_WARN, "Noise(): Server, set private key error, " + noiseUtil.errToString(err));
			return 0;
		}
	}

	/* Set the remote public key for the client */
	if(noise_handshakestate_needs_remote_public_key(handshake)) {
		dh = noise_handshakestate_get_remote_public_key_dh(handshake);
		dhId = noise_dhstate_get_dh_id(dh);
		if(dhId == NOISE_DH_CURVE25519) {
			err = noise_dhstate_set_public_key
				(dh, clientKey25519, sizeof(clientKey25519));
		}
		else {
			err = NOISE_ERROR_UNKNOWN_ID;
		}
		if(err != NOISE_ERROR_NONE) {
			log->printf(LOG_LEVEL_WARN, "Noise(): Server, set public key error, " + noiseUtil.errToString(err));
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
	free(nid);
	client->disconnect();
	delete client;
	writeInfoThread->join();
	receiveThread->join();
	log->printf(LOG_LEVEL_INFO, "Cleaned up noise server");
}
