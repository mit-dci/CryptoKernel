/*
 * NoiseClient.h
 *
 *  Created on: Aug 2, 2018
 *      Author: luke
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
	bool handshakeComplete;

	NoiseCipherState* send_cipher;
	NoiseCipherState* recv_cipher;
	std::mutex handshakeCompleteMutex;

	NoiseUtil noiseUtil;

public:
	NoiseClient(sf::TcpSocket* server, std::string ipAddress, uint64_t port, CryptoKernel::Log* log);

	int getProtocolId(EchoProtocolId* id, const char* name);
	int initializeHandshake(NoiseHandshakeState *handshake, const void *prologue, size_t prologue_len);
	void writeInfo();
	void setHandshakeComplete(bool complete);
	bool getHandshakeComplete();
	void recievePacket(sf::Packet packet);

	virtual ~NoiseClient();
};

#endif /* SRC_KERNEL_NOISECLIENT_H_ */
