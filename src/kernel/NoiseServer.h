/*
 * NoiseServer.h
 *
 *  Created on: Aug 2, 2018
 *      Author: luke
 */

#ifndef SRC_KERNEL_NOISESERVER_H_
#define SRC_KERNEL_NOISESERVER_H_

#include <stdio.h>
#include <SFML/Network.hpp>

#include "NoiseUtil.h"
#include "log.h"

class NoiseServer {
public:
	uint8_t client_key_25519[CURVE25519_KEY_LEN];
	uint8_t server_key_25519[CURVE25519_KEY_LEN];
	uint8_t client_key_448[CURVE448_KEY_LEN];
	uint8_t server_key_448[CURVE448_KEY_LEN];

	sf::TcpSocket* client;
	CryptoKernel::Log* log;
	uint64_t port;

	bool recievedId;
	bool receivedPubKey;
	size_t message_size;
	NoiseBuffer mbuf;
	NoiseHandshakeState *handshake;
	EchoProtocolId id;

	std::mutex handshakeMutex;

	std::unique_ptr<std::thread> writeInfoThread;

	NoiseCipherState* send_cipher;
	NoiseCipherState* recv_cipher;

	NoiseUtil noiseUtil;

	bool handshakeComplete;
	std::mutex handshakeCompleteMutex;

public:
	NoiseServer(sf::TcpSocket* client, uint64_t port, CryptoKernel::Log* log);

	void writeInfo();
	void recievePacket(sf::Packet packet);
	void setHandshakeComplete(bool complete);
	bool getHandshakeComplete();
	int initializeHandshake(NoiseHandshakeState* handshake, const NoiseProtocolId* nid, const void* prologue, size_t prologue_len);

	virtual ~NoiseServer();
};

#endif /* SRC_KERNEL_NOISESERVER_H_ */
