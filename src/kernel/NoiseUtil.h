/*
 * NoiseUtil.h
 *
 *  Created on: Aug 2, 2018
 *      Author: luke
 */

#ifndef SRC_KERNEL_NOISEUTIL_H_
#define SRC_KERNEL_NOISEUTIL_H_

#include <noise/protocol.h>
#include <stdio.h>
#include <string.h>
#include <thread>

#define MAX_DH_KEY_LEN 2048
#define CURVE25519_KEY_LEN 32
#define CURVE448_KEY_LEN 56
#define MAX_DH_KEY_LEN 2048
#define MAX_MESSAGE_LEN 4096

#define ECHO_PATTERN_NN             0x00
#define ECHO_PATTERN_KN             0x01
#define ECHO_PATTERN_NK             0x02
#define ECHO_PATTERN_KK             0x03
#define ECHO_PATTERN_NX             0x04
#define ECHO_PATTERN_KX             0x05
#define ECHO_PATTERN_XN             0x06
#define ECHO_PATTERN_IN             0x07
#define ECHO_PATTERN_XK             0x08
#define ECHO_PATTERN_IK             0x09
#define ECHO_PATTERN_XX             0x0A
#define ECHO_PATTERN_IX             0x0B
#define ECHO_PATTERN_HFS            0x80

#define ECHO_CIPHER_CHACHAPOLY      0x00
#define ECHO_CIPHER_AESGCM          0x01

#define ECHO_DH_25519               0x00
#define ECHO_DH_448                 0x01
#define ECHO_DH_NEWHOPE             0x02
#define ECHO_DH_MASK                0x0F

#define ECHO_HYBRID_NONE            0x00
#define ECHO_HYBRID_25519           0x10
#define ECHO_HYBRID_448             0x20
#define ECHO_HYBRID_NEWHOPE         0x30
#define ECHO_HYBRID_MASK            0xF0

#define ECHO_HASH_SHA256            0x00
#define ECHO_HASH_SHA512            0x01
#define ECHO_HASH_BLAKE2s           0x02
#define ECHO_HASH_BLAKE2b           0x03

#define ECHO_PSK_DISABLED           0x00
#define ECHO_PSK_ENABLED            0x01

typedef struct
{
    uint8_t psk;
    uint8_t pattern;
    uint8_t cipher;
    uint8_t dh;
    uint8_t hash;

} EchoProtocolId;

class NoiseUtil {
public:
	NoiseUtil();

	int loadPrivateKey(const char* filename, uint8_t* key, size_t len);
	int loadPublicKey(const char* filename, uint8_t* key, size_t len);
	int toNoiseProtocolId(NoiseProtocolId* nid, const EchoProtocolId* id);

	virtual ~NoiseUtil();
};

#endif /* SRC_KERNEL_NOISEUTIL_H_ */
