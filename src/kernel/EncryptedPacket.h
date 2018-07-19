/*
 * EncryptedPacket.h
 *
 *  Created on: Jul 19, 2018
 *      Author: luke
 */

#ifndef SRC_KERNEL_ENCRYPTEDPACKET_H_
#define SRC_KERNEL_ENCRYPTEDPACKET_H_

#include <SFML/Network.hpp>

class EncryptedPacket : public sf::Packet {
    virtual const void* onSend(std::size_t& size) {
        const void* srcData = getData();
        std::size_t srcSize = getDataSize();
        return srcData;
    }

    virtual void onReceive(const void* data, std::size_t size) {
        std::size_t dstSize;
        append(data, dstSize);
    }
};

#endif /* SRC_KERNEL_ENCRYPTEDPACKET_H_ */
