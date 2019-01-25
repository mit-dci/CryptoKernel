/*  CryptoKernel - A library for creating blockchain based digital currency
    Copyright (C) 2019  Luke Horgan, James Lovejoy

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "NoiseServer.h"

NoiseServer::NoiseServer(sf::TcpSocket* client, CryptoKernel::Log* log) {
	sendCipher = 0;
	recvCipher = 0;

	this->client = client;
	this->log = log;

	addr = client->getRemoteAddress().toString();

	handshake = 0;
	messageSize = 0;
	receivedId = false;
	receivedPubKey = false;

	handshakeComplete = false;
	handshakeSuccess = false;

	if(noise_init() != NOISE_ERROR_NONE) {
		log->printf(LOG_LEVEL_WARN, "Noise(): Server (for " + addr + "), noise initialization failed. " + addr);
		return;
	}

	if(!noiseUtil.loadPrivateKey("keys/server_key_25519", serverKey25519, sizeof(serverKey25519))) {
		log->printf(LOG_LEVEL_INFO, "could not load server key 25519");
		uint8_t* serverPubKey;
		uint8_t* serverPrivKey;
		if(!noiseUtil.writeKeys("keys/server_key_25519.pub", "keys/server_key_25519", &serverPubKey, &serverPrivKey)) {
			log->printf(LOG_LEVEL_INFO, "Could not write server keys. " + addr);
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

	memset(&nid, 0, sizeof(NoiseProtocolId));
	nid.prefix_id = NOISE_PREFIX_STANDARD;
	nid.pattern_id = NOISE_PATTERN_XX;
	nid.cipher_id = NOISE_CIPHER_AESGCM;
	nid.dh_id = NOISE_DH_CURVE25519;
	nid.hash_id = NOISE_HASH_SHA256;
	nid.hybrid_id = NOISE_DH_NONE;

	writeInfoThread.reset(new std::thread(&NoiseServer::writeInfo, this)); // start the write info thread
    receiveThread.reset(new std::thread(&NoiseServer::receiveWrapper, this));
}

void NoiseServer::writeInfo() {
	uint8_t message[MAX_MESSAGE_LEN + 2];
	unsigned int ok = 1;

	unsigned int count = 0;

	while(!getHandshakeComplete()) { // it might fail in another thread, and so become "complete"
		int action;
		{
			std::lock_guard<std::mutex> lock(handshakeMutex);

			if(!receivedPubKey) {
				count++;
				if(count > 6000) {
					break;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
				continue;
			}

			action = noise_handshakestate_get_action(handshake);
		}

		if(action == NOISE_ACTION_WRITE_MESSAGE) {
			log->printf(LOG_LEVEL_INFO, "Noise(): Server writing message. " + addr + " " + addr);
			/* Write the next handshake message with a zero-length payload */
			handshakeMutex.lock();
			noise_buffer_set_output(mbuf, message + 2, sizeof(message) - 2);
			int err = noise_handshakestate_write_message(handshake, &mbuf, NULL);
			
			if (err != NOISE_ERROR_NONE) {
				log->printf(LOG_LEVEL_WARN, "Noise(): Server, error writing message: " + noiseUtil.errToString(err) + " " + addr);
				setHandshakeComplete(true, false);
				ok = 0;
			}
			message[0] = (uint8_t)(mbuf.size >> 8);
			message[1] = (uint8_t)mbuf.size;

			sf::Packet packet;
			packet.append(message, mbuf.size + 2);
			handshakeMutex.unlock();

			if(client->send(packet) != sf::Socket::Done) {
				log->printf(LOG_LEVEL_WARN, "Noise(): Server (for " + addr + "), something went wrong sending packet to client");
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
		log->printf(LOG_LEVEL_INFO, "Noise(): Server (for " + addr + "), handshake failed.");
		ok = 0;
	}
	handshakeMutex.unlock();

	/* Split out the two CipherState objects for send and receive */
	int err;
	if(ok) {
		handshakeMutex.lock();
		err = noise_handshakestate_split(handshake, &sendCipher, &recvCipher);
		if (err != NOISE_ERROR_NONE) {
			log->printf(LOG_LEVEL_WARN, "Noise(): Server (for " + addr + "), split failed: " + noiseUtil.errToString(err));
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
    bool quitThread = false;

	sf::SocketSelector selector;
	selector.add(*client);

    while(!quitThread && !getHandshakeComplete()) {
		if(selector.wait(sf::seconds(1))) {
			sf::Packet packet;
			const auto status = client->receive(packet);
			if(status == sf::Socket::Done) {
				receivePacket(packet);
			}
			else if(status == sf::Socket::Disconnected) {
				log->printf(LOG_LEVEL_INFO, "Noise(): Server, " + addr + " disconnected.");
				quitThread = true;
				setHandshakeComplete(true, false);
			}
			else {
				log->printf(LOG_LEVEL_INFO, "Noise(): Server for " +  addr + " encountered error receiving packet.");
				quitThread = true;
				setHandshakeComplete(true, false);
			}
		}
	}

	selector.remove(*client);
}

void NoiseServer::receivePacket(sf::Packet packet) {
	int err;
	uint8_t message[MAX_MESSAGE_LEN + 2];

	handshakeMutex.lock();
	if(!receivedPubKey) {
		receivedPubKey = true;
		handshakeMutex.unlock();
		log->printf(LOG_LEVEL_INFO, "Noise(): Server for " + addr + " received a packet, hopefully the public key.");
		memcpy(clientKey25519, packet.getData(), packet.getDataSize());

		/* Convert the echo protocol identifier into a Noise protocol identifier */
		bool ok = true;

		/* Create a HandshakeState object to manage the server's handshake */
		if(ok) {
			std::lock_guard<std::mutex> hc(handshakeMutex);
			err = noise_handshakestate_new_by_id
				(&handshake, &nid, NOISE_ROLE_RESPONDER);
			if(err != NOISE_ERROR_NONE) {
				log->printf(LOG_LEVEL_INFO, "Noise(): Server, create handshake: " + noiseUtil.errToString(err) + "(" + addr + ")");
				setHandshakeComplete(true, false);
				return;
			}
		}

		/* Set all keys that are needed by the client's requested echo protocol */
		if(ok) {
			std::lock_guard<std::mutex> hm(handshakeMutex);
			if (!initializeHandshake(handshake, sizeof(prologue))) {
				log->printf(LOG_LEVEL_WARN, "Noise(): Server for " +  addr + ", couldn't initialize handshake.");
				setHandshakeComplete(true, false);
				return;
			}
		}

		/* Start the handshake */
		if(ok) {
			std::lock_guard<std::mutex> hc(handshakeMutex);
			err = noise_handshakestate_start(handshake);
			if (err != NOISE_ERROR_NONE) {
				log->printf(LOG_LEVEL_INFO, "Noise(): Server for " + addr + " couldn't start handshake: " + noiseUtil.errToString(err));
				setHandshakeComplete(true, false);
				return;
			}
		}
	}
	else {
		int action = noise_handshakestate_get_action(handshake);
		if(action == NOISE_ACTION_READ_MESSAGE) {
			log->printf(LOG_LEVEL_INFO, "Noise(): Server, reading message. " + addr);
			messageSize = packet.getDataSize();
			memcpy(message, packet.getData(), packet.getDataSize());

			noise_buffer_set_input(mbuf, message + 2, messageSize - 2);
			err = noise_handshakestate_read_message(handshake, &mbuf, NULL);
			if (err != NOISE_ERROR_NONE) {
				log->printf(LOG_LEVEL_WARN, "Noise(): Server for " + addr + ", read handshake error: " + noiseUtil.errToString(err));
				setHandshakeComplete(true, false);
			}
		}
		handshakeMutex.unlock();
	}
}

/* Initialize's the handshake with all necessary keys */
int NoiseServer::initializeHandshake(NoiseHandshakeState* handshake, size_t prologueLen) {
	NoiseDHState *dh;
	int dhId;
	int err;

	/* Set the prologue first */
	err = noise_handshakestate_set_prologue(handshake, &prologue, prologueLen);
	if(err != NOISE_ERROR_NONE) {
		log->printf(LOG_LEVEL_WARN, "Noise(): Server for " + addr + ", prologue error: " + noiseUtil.errToString(err));
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
			log->printf(LOG_LEVEL_WARN, "Noise(): Server for " + addr + ", set private key error, " + noiseUtil.errToString(err));
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
			log->printf(LOG_LEVEL_WARN, "Noise(): Server for " + addr + ", set public key error." + noiseUtil.errToString(err));
			return 0;
		}
	}

	/* Ready to go */
	return 1;
}

void NoiseServer::setHandshakeComplete(bool complete, bool success) {
	if(success) {
		log->printf(LOG_LEVEL_INFO, "Noise(): Server handshake succeeded, " + addr);
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
	log->printf(LOG_LEVEL_INFO, "Cleaning up noise server for " + addr);
	if(!getHandshakeComplete()) {
		setHandshakeComplete(true, false);
	}

	if(writeInfoThread) {
		writeInfoThread->join();
	}

	if(receiveThread) {
		receiveThread->join();
	}

	client->disconnect();
	delete client;

	log->printf(LOG_LEVEL_INFO, "Cleaned up noise server for " + addr);
}
