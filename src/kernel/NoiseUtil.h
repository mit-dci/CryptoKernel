/*
 * NoiseUtil.h
 *
 *  Created on: Aug 2, 2018
 *      Author: Luke Horgan
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
