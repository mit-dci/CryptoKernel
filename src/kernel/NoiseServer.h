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
	uint8_t clientKey25519[CURVE25519_KEY_LEN];
	uint8_t serverKey25519[CURVE25519_KEY_LEN];
	sf::TcpSocket* client;
	CryptoKernel::Log* log;
	uint64_t port;
	bool receivedId;
	bool receivedPubKey;
	size_t messageSize;
	NoiseBuffer mbuf;
	NoiseHandshakeState *handshake;
	Prologue prologue;
	std::mutex handshakeMutex;
	std::unique_ptr<std::thread> writeInfoThread;
	NoiseCipherState* sendCipher;
	NoiseCipherState* recvCipher;
	NoiseUtil noiseUtil;
	bool handshakeComplete;
	bool handshakeSuccess;
	std::mutex handshakeCompleteMutex;
	NoiseProtocolId nid;
	std::string addr;
	std::unique_ptr<std::thread> receiveThread;

	void writeInfo();
	void setHandshakeComplete(bool complete, bool success);
	int initializeHandshake(NoiseHandshakeState* handshake, size_t prologue_len);
	void receiveWrapper();
	void receivePacket(sf::Packet packet);
	
	NoiseServer(sf::TcpSocket* client, CryptoKernel::Log* log);

	bool getHandshakeComplete();
	bool getHandshakeSuccess();

	virtual ~NoiseServer();
};

#endif /* SRC_KERNEL_NOISESERVER_H_ */
