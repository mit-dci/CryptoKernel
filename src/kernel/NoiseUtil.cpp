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

#include "NoiseUtil.h"

#include <unistd.h>
#include <stdlib.h>
#include <string>

NoiseUtil::NoiseUtil() {
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

/* Saves a binary private key to a file.  Returns non-zero if OK. */
int NoiseUtil::savePrivateKey(const char *filename, const uint8_t *key, size_t len) {
    FILE *file = fopen(filename, "wb");
    size_t posn;
    if (!file) {
        perror(filename);
        return 0;
    }
    for (posn = 0; posn < len; ++posn)
        putc(key[posn], file);
    fclose(file);
    return 1;
}

/* Saves a base64-encoded public key to a file.  Returns non-zero if OK. */
int NoiseUtil::savePublicKey(const char *filename, const uint8_t *key, size_t len) {
    static char const base64_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    FILE *file = fopen(filename, "wb");
    size_t posn = 0;
    uint32_t group;
    if (!file) {
        perror(filename);
        return 0;
    }
    while ((len - posn) >= 3) {
        group = (((uint32_t)(key[posn])) << 16) |
                (((uint32_t)(key[posn + 1])) << 8) |
                 ((uint32_t)(key[posn + 2]));
        putc(base64_chars[(group >> 18) & 0x3F], file);
        putc(base64_chars[(group >> 12) & 0x3F], file);
        putc(base64_chars[(group >> 6) & 0x3F], file);
        putc(base64_chars[group & 0x3F], file);
        posn += 3;
    }
    if ((len - posn) == 2) {
        group = (((uint32_t)(key[posn])) << 16) |
                (((uint32_t)(key[posn + 1])) << 8);
        putc(base64_chars[(group >> 18) & 0x3F], file);
        putc(base64_chars[(group >> 12) & 0x3F], file);
        putc(base64_chars[(group >> 6) & 0x3F], file);
        putc('=', file);
    } else if ((len - posn) == 1) {
        group = ((uint32_t)(key[posn])) << 16;
        putc(base64_chars[(group >> 18) & 0x3F], file);
        putc(base64_chars[(group >> 12) & 0x3F], file);
        putc('=', file);
        putc('=', file);
    }
    fclose(file);
    return 1;
}

// generate keys and write them to disk
int NoiseUtil::writeKeys(const char* pubKeyName, const char* privKeyName, uint8_t** pub_key, uint8_t** priv_key) {
	NoiseDHState* dh;
	int err = noise_dhstate_new_by_name(&dh, "25519");
	if(err != NOISE_ERROR_NONE) {
		return 0;
	}
	err = noise_dhstate_generate_keypair(dh);
	if(err != NOISE_ERROR_NONE) {
		noise_dhstate_free(dh);
		return 0;
	}

	/* Fetch the keypair to be saved */
	size_t priv_key_len = noise_dhstate_get_private_key_length(dh);
	size_t pub_key_len = noise_dhstate_get_public_key_length(dh);

	*priv_key = (uint8_t *)malloc(priv_key_len);
	*pub_key = (uint8_t *)malloc(pub_key_len);

	if (!priv_key || !pub_key) {
		fprintf(stderr, "Out of memory\n");
		return 0;
	}
	err = noise_dhstate_get_keypair
		(dh, *priv_key, priv_key_len, *pub_key, pub_key_len);
	if (err != NOISE_ERROR_NONE) {
		noise_perror("get keypair for saving", err);
		return 0;
	}

	int ok = 1;

	/* Save the keys */
	ok = savePrivateKey(privKeyName, *priv_key, priv_key_len);
	if (ok)
		ok = savePublicKey(pubKeyName, *pub_key, pub_key_len);

	/* Clean up */
	noise_dhstate_free(dh);

	if (!ok) {
		unlink(privKeyName);
		unlink(pubKeyName);
	}

	return ok;
}

std::string NoiseUtil::errToString(int err) {
	char* buf = new char[4096];
	noise_strerror(err, buf, 4096);
	std::string errString(buf);
	delete[] buf;
	return errString;
}

NoiseUtil::~NoiseUtil() {
}
