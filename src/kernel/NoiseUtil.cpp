/*
 * NoiseUtil.cpp
 *
 *  Created on: Aug 2, 2018
 *      Author: luke
 */

#include "NoiseUtil.h"

NoiseUtil::NoiseUtil() {
	// TODO Auto-generated constructor stub
}

/* Loads a binary private key from a file.  Returns non-zero if OK. */
int NoiseUtil::loadPrivateKey(const char* filename, uint8_t* key, size_t len) {
	FILE *file = fopen(filename, "rb");
	size_t posn = 0;
	int ch;
	if (len > MAX_DH_KEY_LEN) {
		fprintf(stderr, "private key length is not supported\n");
		return 0;
	}
	if (!file) {
		perror(filename);
		return 0;
	}
	while ((ch = getc(file)) != EOF) {
		if (posn >= len) {
			fclose(file);
			fprintf(stderr, "%s: private key value is too long\n", filename);
			return 0;
		}
		key[posn++] = (uint8_t)ch;
	}
	if (posn < len) {
		fclose(file);
		fprintf(stderr, "%s: private key value is too short\n", filename);
		return 0;
	}
	fclose(file);
	return 1;
}

/* Loads a base64-encoded public key from a file.  Returns non-zero if OK. */
int NoiseUtil::loadPublicKey(const char *filename, uint8_t *key, size_t len) {
	FILE *file = fopen(filename, "rb");
	uint32_t group = 0;
	size_t group_size = 0;
	uint32_t digit = 0;
	size_t posn = 0;
	int ch;
	if (len > MAX_DH_KEY_LEN) {
		fprintf(stderr, "public key length is not supported\n");
		return 0;
	}
	if (!file) {
		perror(filename);
		return 0;
	}
	while ((ch = getc(file)) != EOF) {
		if (ch >= 'A' && ch <= 'Z') {
			digit = ch - 'A';
		} else if (ch >= 'a' && ch <= 'z') {
			digit = ch - 'a' + 26;
		} else if (ch >= '0' && ch <= '9') {
			digit = ch - '0' + 52;
		} else if (ch == '+') {
			digit = 62;
		} else if (ch == '/') {
			digit = 63;
		} else if (ch == '=') {
			break;
		} else if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
			fclose(file);
			fprintf(stderr, "%s: invalid character in public key file\n", filename);
			return 0;
		}
		group = (group << 6) | digit;
		if (++group_size >= 4) {
			if ((len - posn) < 3) {
				fclose(file);
				fprintf(stderr, "%s: public key value is too long\n", filename);
				return 0;
			}
			group_size = 0;
			key[posn++] = (uint8_t)(group >> 16);
			key[posn++] = (uint8_t)(group >> 8);
			key[posn++] = (uint8_t)group;
		}
	}
	if (group_size == 3) {
		if ((len - posn) < 2) {
			fclose(file);
			fprintf(stderr, "%s: public key value is too long\n", filename);
			return 0;
		}
		key[posn++] = (uint8_t)(group >> 10);
		key[posn++] = (uint8_t)(group >> 2);
	} else if (group_size == 2) {
		if ((len - posn) < 1) {
			fclose(file);
			fprintf(stderr, "%s: public key value is too long\n", filename);
			return 0;
		}
		key[posn++] = (uint8_t)(group >> 4);
	}
	if (posn < len) {
		fclose(file);
		fprintf(stderr, "%s: public key value is too short\n", filename);
		return 0;
	}
	fclose(file);
	return 1;
}

/* Convert an Echo protocol id into a Noise protocol id */
int NoiseUtil::toNoiseProtocolId(NoiseProtocolId* nid, const EchoProtocolId* id) {
	int ok = 1;

	memset(nid, 0, sizeof(NoiseProtocolId));

	switch (id->psk) {
	case ECHO_PSK_DISABLED:         nid->prefix_id = NOISE_PREFIX_STANDARD; break;
	case ECHO_PSK_ENABLED:          nid->prefix_id = NOISE_PREFIX_PSK; break;
	default:                        ok = 0;
	}

	switch (id->pattern) {
	case ECHO_PATTERN_NN:           nid->pattern_id = NOISE_PATTERN_NN; break;
	case ECHO_PATTERN_KN:           nid->pattern_id = NOISE_PATTERN_KN; break;
	case ECHO_PATTERN_NK:           nid->pattern_id = NOISE_PATTERN_NK; break;
	case ECHO_PATTERN_KK:           nid->pattern_id = NOISE_PATTERN_KK; break;
	case ECHO_PATTERN_NX:           nid->pattern_id = NOISE_PATTERN_NX; break;
	case ECHO_PATTERN_KX:           nid->pattern_id = NOISE_PATTERN_KX; break;
	case ECHO_PATTERN_XN:           nid->pattern_id = NOISE_PATTERN_XN; break;
	case ECHO_PATTERN_IN:           nid->pattern_id = NOISE_PATTERN_IN; break;
	case ECHO_PATTERN_XK:           nid->pattern_id = NOISE_PATTERN_XK; break;
	case ECHO_PATTERN_IK:           nid->pattern_id = NOISE_PATTERN_IK; break;
	case ECHO_PATTERN_XX:           nid->pattern_id = NOISE_PATTERN_XX; break;
	case ECHO_PATTERN_IX:           nid->pattern_id = NOISE_PATTERN_IX; break;
	case ECHO_PATTERN_NN | ECHO_PATTERN_HFS:    nid->pattern_id = NOISE_PATTERN_NN_HFS; break;
	case ECHO_PATTERN_KN | ECHO_PATTERN_HFS:    nid->pattern_id = NOISE_PATTERN_KN_HFS; break;
	case ECHO_PATTERN_NK | ECHO_PATTERN_HFS:    nid->pattern_id = NOISE_PATTERN_NK_HFS; break;
	case ECHO_PATTERN_KK | ECHO_PATTERN_HFS:    nid->pattern_id = NOISE_PATTERN_KK_HFS; break;
	case ECHO_PATTERN_NX | ECHO_PATTERN_HFS:    nid->pattern_id = NOISE_PATTERN_NX_HFS; break;
	case ECHO_PATTERN_KX | ECHO_PATTERN_HFS:    nid->pattern_id = NOISE_PATTERN_KX_HFS; break;
	case ECHO_PATTERN_XN | ECHO_PATTERN_HFS:    nid->pattern_id = NOISE_PATTERN_XN_HFS; break;
	case ECHO_PATTERN_IN | ECHO_PATTERN_HFS:    nid->pattern_id = NOISE_PATTERN_IN_HFS; break;
	case ECHO_PATTERN_XK | ECHO_PATTERN_HFS:    nid->pattern_id = NOISE_PATTERN_XK_HFS; break;
	case ECHO_PATTERN_IK | ECHO_PATTERN_HFS:    nid->pattern_id = NOISE_PATTERN_IK_HFS; break;
	case ECHO_PATTERN_XX | ECHO_PATTERN_HFS:    nid->pattern_id = NOISE_PATTERN_XX_HFS; break;
	case ECHO_PATTERN_IX | ECHO_PATTERN_HFS:    nid->pattern_id = NOISE_PATTERN_IX_HFS; break;
	default:                        ok = 0;
	}

	switch (id->cipher) {
	case ECHO_CIPHER_CHACHAPOLY:    nid->cipher_id = NOISE_CIPHER_CHACHAPOLY; break;
	case ECHO_CIPHER_AESGCM:        nid->cipher_id = NOISE_CIPHER_AESGCM; break;
	default:                        ok = 0;
	}

	switch (id->dh & ECHO_DH_MASK) {
	case ECHO_DH_25519:             nid->dh_id = NOISE_DH_CURVE25519; break;
	case ECHO_DH_448:               nid->dh_id = NOISE_DH_CURVE448; break;
	case ECHO_DH_NEWHOPE:           nid->dh_id = NOISE_DH_NEWHOPE; break;
	default:                        ok = 0;
	}

	switch (id->dh & ECHO_HYBRID_MASK) {
	case ECHO_HYBRID_25519:         nid->hybrid_id = NOISE_DH_CURVE25519; break;
	case ECHO_HYBRID_448:           nid->hybrid_id = NOISE_DH_CURVE448; break;
	case ECHO_HYBRID_NEWHOPE:       nid->hybrid_id = NOISE_DH_NEWHOPE; break;
	case ECHO_HYBRID_NONE:          nid->hybrid_id = NOISE_DH_NONE; break;
	default:                        ok = 0;
	}

	switch (id->hash) {
	case ECHO_HASH_SHA256:          nid->hash_id = NOISE_HASH_SHA256; break;
	case ECHO_HASH_SHA512:          nid->hash_id = NOISE_HASH_SHA512; break;
	case ECHO_HASH_BLAKE2s:         nid->hash_id = NOISE_HASH_BLAKE2s; break;
	case ECHO_HASH_BLAKE2b:         nid->hash_id = NOISE_HASH_BLAKE2b; break;
	default:                        ok = 0;
	}

	return ok;
}

NoiseUtil::~NoiseUtil() {
	// TODO Auto-generated destructor stub
}

