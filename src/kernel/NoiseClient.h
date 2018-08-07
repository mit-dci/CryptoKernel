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
	std::shared_ptr<sf::TcpSocket> server;
	CryptoKernel::Log* log;
	std::string ipAddress;
	uint64_t port;

	std::string client_public_key;
	std::string server_public_key;
	std::string client_private_key;
	std::string psk;
	std::string psk_file;

	uint8_t clientKey25519[CURVE25519_KEY_LEN];

	std::mutex handshakeMutex;

	NoiseHandshakeState* handshake;
	EchoProtocolId id;
	std::unique_ptr<std::thread> writeInfoThread;
	bool sentId;
	bool handshakeComplete;
	bool handshakeSuccess;

	NoiseCipherState* send_cipher;
	NoiseCipherState* recv_cipher;
	std::mutex handshakeCompleteMutex;

	NoiseUtil noiseUtil;

	uint8_t* priv_key;
	uint8_t* pub_key;

public:
	NoiseClient(std::shared_ptr<sf::TcpSocket> server, std::string ipAddress, uint64_t port, CryptoKernel::Log* log);

	int getProtocolId(EchoProtocolId* id, const char* name);
	int initializeHandshake(NoiseHandshakeState *handshake, const void *prologue, size_t prologue_len);
	void writeInfo();
	void setHandshakeComplete(bool complete, bool success);
	bool getHandshakeComplete();
	void receivePacket(sf::Packet packet);
	bool getHandshakeSuccess();

	virtual ~NoiseClient();
};

#endif /* SRC_KERNEL_NOISECLIENT_H_ */
