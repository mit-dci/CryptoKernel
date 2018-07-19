/*
 * EncryptedPacket.h
 *
 *  Created on: Jul 19, 2018
 *      Author: luke
 */

#ifndef SRC_KERNEL_ENCRYPTEDPACKET_H_
#define SRC_KERNEL_ENCRYPTEDPACKET_H_

#include <SFML/Network.hpp>

class EncryptedPacket : public sf::Packet
{
    virtual const void* onSend(std::size_t& size) {
        /*const void* srcData = getData();
        std::size_t srcSize = getDataSize();
        return compressTheData(srcData, srcSize, &size); // this is a fake function, of course :)*/
    	return "hello";
    }
    virtual void onReceive(const void* data, std::size_t size) {
        /*std::size_t dstSize;
        const void* dstData = uncompressTheData(data, size, &dstSize); // this is a fake function, of course :)
        append(dstData, dstSize);*/
    }
};

#endif /* SRC_KERNEL_ENCRYPTEDPACKET_H_ */
