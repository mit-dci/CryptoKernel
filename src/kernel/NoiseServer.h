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
