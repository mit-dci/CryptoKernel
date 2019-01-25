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

#ifndef SRC_KERNEL_NOISECLIENT_H_
#define SRC_KERNEL_NOISECLIENT_H_

#include <stdio.h>
#include <SFML/Network.hpp>

#include "NoiseUtil.h"
#include "log.h"

class NoiseClient {
private:
	sf::TcpSocket* server;
	CryptoKernel::Log* log;
	std::string ipAddress;
	std::string addr;
	std::string clientPrivateKey;
	uint8_t clientKey25519[CURVE25519_KEY_LEN];
	std::mutex handshakeMutex;
	NoiseHandshakeState* handshake;
	Prologue prologue;
	std::unique_ptr<std::thread> writeInfoThread;
	bool sentId;
	bool handshakeComplete;
	bool handshakeSuccess;
	std::mutex handshakeCompleteMutex;
	NoiseUtil noiseUtil;
	std::unique_ptr<std::thread> receiveThread;

	int getProtocolId(Prologue* id, const char* name);
	int initializeHandshake(NoiseHandshakeState *handshake, const void *prologue, size_t prologue_len);
	void writeInfo();
	void setHandshakeComplete(bool complete, bool success);
    void receiveWrapper();
	void receivePacket(sf::Packet& packet);

public:
	NoiseClient(sf::TcpSocket* server, CryptoKernel::Log* log);

	bool getHandshakeComplete();
	bool getHandshakeSuccess();

	virtual ~NoiseClient();

	NoiseCipherState* send_cipher;
	NoiseCipherState* recv_cipher;
};

#endif /* SRC_KERNEL_NOISECLIENT_H_ */
