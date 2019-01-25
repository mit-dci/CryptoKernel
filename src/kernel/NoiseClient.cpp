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

#include "NoiseClient.h"
#include <unistd.h>
#include <stdlib.h>

NoiseClient::NoiseClient(sf::TcpSocket* server, CryptoKernel::Log* log) {
	this->server = server;
	this->log = log;
	this->ipAddress = ipAddress;

	addr = server->getRemoteAddress().toString();

	clientPrivateKey = "keys/client_key_25519";

	send_cipher = 0;
	recv_cipher = 0;

	sentId = false;

	handshakeComplete = false;
	handshakeSuccess = false;

	if(noise_init() != NOISE_ERROR_NONE) {
		log->printf(LOG_LEVEL_WARN, "Noise(): Client (of " + addr + "), Noise initialization failed");
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
		log->printf(LOG_LEVEL_WARN, "Client (of " + addr + "), error: " + noiseUtil.errToString(err));
		return;
	}

	log->printf(LOG_LEVEL_INFO, "Noise(): Client (of " + addr + "), starting writeInfo.");
	writeInfoThread.reset(new std::thread(&NoiseClient::writeInfo, this)); // start the write info thread
    receiveThread.reset(new std::thread(&NoiseClient::receiveWrapper, this));
}

void NoiseClient::writeInfo() {
	int action;
	NoiseBuffer mbuf;
	int err, ok;
	size_t message_size;

	uint8_t message[MAX_MESSAGE_LEN + 2];

	ok = 1;

	bool sentPubKey = false;

	while(!getHandshakeComplete()) { // it might fail in another thread, and so become "complete"
		if(server->getRemoteAddress() == sf::IpAddress::None) {
			log->printf(LOG_LEVEL_WARN, "Noise(): Client (of " + addr + "), the remote port has been invalidated");
			setHandshakeComplete(true, false);
			ok = 0;
			continue;
		}

		if(!sentPubKey) { // we need to share our public key over the network
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
				log->printf(LOG_LEVEL_WARN, "Noise(): Client (of " + addr + "), error initializing handshake.");
				ok = 0;
				handshakeMutex.unlock();
				setHandshakeComplete(true, false);
				continue;
			}
			handshakeMutex.unlock();

			pubKeyPacket.append(clientKey25519, sizeof(clientKey25519));

			if(server->send(pubKeyPacket) != sf::Socket::Done) { // not useful right now, but could be if we go non-blocking
				continue; // keep sending the public key until it goes through
			}

			sentPubKey = true;
			err = noise_handshakestate_start(handshake);
			if(err != NOISE_ERROR_NONE) {
				log->printf(LOG_LEVEL_WARN, "Noise(): Client (of " + addr + "), start handshake error, " + noiseUtil.errToString(err));
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
					log->printf(LOG_LEVEL_WARN, "Noise(): Client (of " + addr + "), handshake failed on write message: " + noiseUtil.errToString(err));
					setHandshakeComplete(true, false);
					ok = 0;
					continue;
				}
				message[0] = (uint8_t)(mbuf.size >> 8);
				message[1] = (uint8_t)mbuf.size;

				log->printf(LOG_LEVEL_INFO, "Noise(): Client (of " + addr + "), sending a packet.");
				sf::Packet packet;
				packet.append(message, mbuf.size + 2);
				if(server->send(packet) != sf::Socket::Done) {
					log->printf(LOG_LEVEL_WARN, "Noise(): Client (of " + addr + ") couldn't send packet.");
					setHandshakeComplete(true, false);
					ok = 0;
					continue;
				}
				else {
					log->printf(LOG_LEVEL_INFO, "Noise(): Client (of " + addr + "), successfully sent packet to");
				}
			}
			else if(action != NOISE_ACTION_READ_MESSAGE) {
				log->printf(LOG_LEVEL_INFO, "Noise(): Client (of " + addr + "), either the handshake succeeded, or it failed");
				break;
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(50)); // again, totally arbitrary
	}

	/* If the action is not "split", then the handshake has failed */
	handshakeMutex.lock();
	if(ok && noise_handshakestate_get_action(handshake) != NOISE_ACTION_SPLIT) {
		log->printf(LOG_LEVEL_WARN, "Noise(): Client (of " + addr + "), protocol handshake failed");
		ok = 0;
	}
	handshakeMutex.unlock();

	/* Split out the two CipherState objects for send and receive */
	if(ok) {
		handshakeMutex.lock();
		err = noise_handshakestate_split(handshake, &send_cipher, &recv_cipher);
		handshakeMutex.unlock();
		if(err != NOISE_ERROR_NONE) {
			log->printf(LOG_LEVEL_WARN, "Noise(): Client (of " + addr + "), split failed: " + noiseUtil.errToString(err));
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
    log->printf(LOG_LEVEL_INFO, "Noise(): Client (of " + addr + "), receive wrapper starting.");
    bool quitThread = false;

	sf::SocketSelector selector;
	selector.add(*server);

    while(!quitThread && !getHandshakeComplete()) {
		if(selector.wait(sf::seconds(1))) {
			sf::Packet packet;
			const auto status = server->receive(packet);
			if(status == sf::Socket::Done) {
				receivePacket(packet);
			}
			else {
				log->printf(LOG_LEVEL_INFO, "Noise(): Client (of " + addr + ") encountered error receiving packet.");
				quitThread = true;
				setHandshakeComplete(true, false);
			}
		}
	}

	selector.remove(*server);
}

void NoiseClient::receivePacket(sf::Packet& packet) {
	log->printf(LOG_LEVEL_INFO, "Noise(): Client (of " + addr + ") receiving a packet.");

	handshakeMutex.lock();
	int action = noise_handshakestate_get_action(handshake);
	handshakeMutex.unlock();
	NoiseBuffer mbuf;
	int err, ok;
	size_t message_size;

	uint8_t message[MAX_MESSAGE_LEN + 2];

	if(action == NOISE_ACTION_READ_MESSAGE) {
		log->printf(LOG_LEVEL_INFO, "Noise(): Client (of " + addr + ") reading a message.");
 
		message_size = packet.getDataSize();
		memcpy(message, packet.getData(), packet.getDataSize());

		noise_buffer_set_input(mbuf, message + 2, message_size - 2);
		handshakeMutex.lock();
		err = noise_handshakestate_read_message(handshake, &mbuf, NULL);
		handshakeMutex.unlock();
		if(err != NOISE_ERROR_NONE) {
			log->printf(LOG_LEVEL_WARN, "Noise(): Client (of " + addr + "), read handshake " + noiseUtil.errToString(err));
			setHandshakeComplete(true, false);
		}
	}
}

void NoiseClient::setHandshakeComplete(bool complete, bool success) {
	if(success) {
		log->printf(LOG_LEVEL_INFO, "Noise(): Client (of " + addr + ") handshake succeeded.");
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
		log->printf(LOG_LEVEL_INFO, "Noise(): Client (of " + addr + "), prologue: " + noiseUtil.errToString(err));
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
				log->printf(LOG_LEVEL_INFO, "Noise(): Client (of " + addr + "), could not allocate space for key.");
				return 0;
			}
			if(!noiseUtil.loadPrivateKey(clientPrivateKey.c_str(), key, keyLen)) {
				log->printf(LOG_LEVEL_WARN, "Noise(): Client (of " + addr + "), could not load private key.");
				noise_free(key, keyLen);
				return 0;
			}
			err = noise_dhstate_set_keypair_private(dh, key, keyLen);
			noise_free(key, keyLen);
			if(err != NOISE_ERROR_NONE) {
				log->printf(LOG_LEVEL_WARN, "Noise(): Set client (of " + addr + ") private key " + noiseUtil.errToString(err));
				return 0;
			}
		}
		else {
			log->printf(LOG_LEVEL_WARN, "Noise(): Client (of " + addr + ") private key required but not provided.");
			return 0;
		}
	}

	// we are not using a remote public key for the server, but it would be initialized right here

	/* Ready to go */
	return 1;
}

NoiseClient::~NoiseClient() {
	log->printf(LOG_LEVEL_INFO, "Cleaning up noise client (of " + addr + ")");
	if(!getHandshakeComplete()) {
		setHandshakeComplete(true, false);
	}
	server->disconnect();
	delete server;
	writeInfoThread->join();
	receiveThread->join();
	log->printf(LOG_LEVEL_INFO, "Cleaned up noise client (of " + addr + ")");
}
