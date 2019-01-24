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

#ifndef SRC_KERNEL_NOISEUTIL_H_
#define SRC_KERNEL_NOISEUTIL_H_

#include <noise/protocol.h>
#include <stdio.h>
#include <string.h>
#include <thread>

#define MAX_DH_KEY_LEN 2048
#define CURVE25519_KEY_LEN 32
#define MAX_DH_KEY_LEN 2048
#define MAX_MESSAGE_LEN 4096

#define PATTERN_XX             0x0A
#define CIPHER_AESGCM          0x01
#define DH_25519               0x00
#define HASH_SHA256            0x00
#define PSK_DISABLED           0x00

struct Prologue {
    uint8_t psk;
    uint8_t pattern;
    uint8_t cipher;
    uint8_t dh;
    uint8_t hash;

};

class NoiseUtil {
public:
	NoiseUtil();

	int loadPrivateKey(const char* filename, uint8_t* key, size_t len);
	int loadPublicKey(const char* filename, uint8_t* key, size_t len);
	int toNoiseProtocolId(NoiseProtocolId* nid, const Prologue* id);
	int savePrivateKey(const char* filename, const uint8_t* key, size_t len);
	int savePublicKey(const char* filename, const uint8_t* key, size_t len);
	int writeKeys(const char* pubKeyName, const char* privKeyName, uint8_t** pub_key, uint8_t** priv_key);
	std::string errToString(int err);

	virtual ~NoiseUtil();
};

#endif /* SRC_KERNEL_NOISEUTIL_H_ */
