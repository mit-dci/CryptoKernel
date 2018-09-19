/*
 * NoiseClient.h
 *
 *  Created on: Aug 2, 2018
 *      Author: Luke Horgan
 */

#ifndef SRC_KERNEL_NOISECLIENT_H_
#define SRC_KERNEL_NOISECLIENT_H_

#include <stdio.h>
#include <SFML/Network.hpp>

#include "NoiseUtil.h"
#include "log.h"

class NoiseClient {
public:
	sf::TcpSocket* server;
	CryptoKernel::Log* log;
	std::string ipAddress;
	uint64_t port;

	std::string clientPrivateKey;

	uint8_t clientKey25519[CURVE25519_KEY_LEN];

	std::mutex handshakeMutex;

	NoiseHandshakeState* handshake;
	Prologue prologue;
	std::unique_ptr<std::thread> writeInfoThread;
	bool sentId;
	bool handshakeComplete;
	bool handshakeSuccess;

	NoiseCipherState* send_cipher;
	NoiseCipherState* recv_cipher;
	std::mutex handshakeCompleteMutex;

	NoiseUtil noiseUtil;

public:
	NoiseClient(sf::TcpSocket* server, std::string ipAddress, uint64_t port, CryptoKernel::Log* log);

	int getProtocolId(Prologue* id, const char* name);
	int initializeHandshake(NoiseHandshakeState *handshake, const void *prologue, size_t prologue_len);
	void writeInfo();
	void setHandshakeComplete(bool complete, bool success);
	bool getHandshakeComplete();
	bool getHandshakeSuccess();

    void receiveWrapper();
	void receivePacket(sf::Packet& packet);
    std::unique_ptr<std::thread> receiveThread;

    virtual ~NoiseClient();
};

#endif /* SRC_KERNEL_NOISECLIENT_H_ */
