#include <iostream>

#include "blockchain.h"
#include "crypto.h"

int main()
{
    CryptoKernel::Blockchain blockchain;

    while(true)
    {
        CryptoKernel::Blockchain::block Block;
        Block = blockchain.generateMiningBlock("-----BEGIN PUBLIC KEY-----MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA0crGv6V1L3BAeX+eBy75HuddNn/jBENmdPmaZsuUzYd/RfAsqd5aupFHa5XSixCBxsLvZzPc0UHzz23C7Q9nnNMrfxdNsVB36RO9LUXVUluY5VpXoJZizFpqSSPQLdzpC/4ETfdqVY5meFe5Q49/Sn7VI3iBcoehUOLa4rbYDKbobe1YJtPNWyVsZ6hnUlR0H97O4DSqfzH7fYoSjIZn4xep7Ow0yO29kClx2VbpKJRifPkcwDUJojqP1BcA7CbVpbddNAwk4ohqEmVFSBoUdjY8ew3P5UOFZzepBwFdoOZhqtzSXWLs3ApOITCWJuHHWwrxRqSXmPLqy7e5knjXpQIDAQAB-----END PUBLIC KEY-----");
        Block.nonce = 0;

        std::string data = CryptoKernel::Storage::toString(blockchain.blockToJson(Block));
        std::cout << data << std::endl;

        CryptoKernel::Crypto crypto;
        do
        {
            Block.nonce += 1;
            Block.PoW = blockchain.calculatePoW(Block);
        }
        while(!hex_greater(Block.target, Block.PoW));

        CryptoKernel::Blockchain::block previousBlock;
        previousBlock = blockchain.getBlock(Block.previousBlockId);
        Block.totalWork = addHex(Block.PoW, previousBlock.totalWork);

        blockchain.submitBlock(Block);

        data = CryptoKernel::Storage::toString(blockchain.blockToJson(Block));
        std::cout << data << std::endl;
    }

    return 0;
}

