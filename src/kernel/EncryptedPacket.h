/*
 * EncryptedPacket.h
 *
 *  Created on: Jul 19, 2018
 *      Author: luke
 */

#ifndef SRC_KERNEL_ENCRYPTEDPACKET_H_
#define SRC_KERNEL_ENCRYPTEDPACKET_H_

#include <SFML/Network.hpp>
#include <noise/protocol.h>

class EncryptedPacket : public sf::Packet {
public:
	NoiseCipherState* send_cipher;
	NoiseCipherState* recv_cipher;
	CryptoKernel::Log* log;

public:
	EncryptedPacket(NoiseCipherState* send_cipher, NoiseCipherState* recv_cipher, CryptoKernel::Log* log) {
		this->send_cipher = send_cipher;
		this->recv_cipher = recv_cipher;
		this->log = log;
	}

    virtual const void* onSend(std::size_t& size) {
    	/*NoiseBuffer mbuf;

    	const void* srcData = getData();
        std::size_t srcSize = getDataSize();

        noise_buffer_set_inout(mbuf, (uint8_t*)srcData, srcSize, 4096);
        int err = noise_cipherstate_encrypt(send_cipher, &mbuf);

        size = mbuf.size;
        return mbuf.data;*/

    	NoiseBuffer mbuf;
    	log->printf(LOG_LEVEL_INFO, "good here");
    	noise_buffer_set_inout(mbuf, (uint8_t*)getData(), getDataSize(), 4096);
    	log->printf(LOG_LEVEL_INFO, "good here too");
		int err = noise_cipherstate_encrypt(send_cipher, &mbuf);
		log->printf(LOG_LEVEL_INFO, "and good here as well");
		if (err != NOISE_ERROR_NONE) {
			noise_perror("SOMETHING WENT WRONG" , err);
			//break;
			return 0;
		}
		log->printf(LOG_LEVEL_INFO, "errr, we're still good here???");
		size = mbuf.size;
		log->printf(LOG_LEVEL_INFO, "and here, we're good here too?");
		return mbuf.data;
    }

    virtual void onReceive(const void* data, std::size_t size) {
    	NoiseBuffer mbuf;

    	std::size_t dstSize;

    	noise_buffer_set_inout(mbuf, (uint8_t*)data, dstSize, 4096);
		int err = noise_cipherstate_decrypt(recv_cipher, &mbuf);

        append(mbuf.data, mbuf.size);
    }
};

#endif /* SRC_KERNEL_ENCRYPTEDPACKET_H_ */
