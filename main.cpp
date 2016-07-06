#include <iostream>

#include "blockchain.h"
#include "crypto.h"

int main()
{
    std::string publicKey1 = "-----BEGIN PUBLIC KEY-----\nMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA0crGv6V1L3BAeX+eBy75\nHuddNn/jBENmdPmaZsuUzYd/RfAsqd5aupFHa5XSixCBxsLvZzPc0UHzz23C7Q9n\nnNMrfxdNsVB36RO9LUXVUluY5VpXoJZizFpqSSPQLdzpC/4ETfdqVY5meFe5Q49/\nSn7VI3iBcoehUOLa4rbYDKbobe1YJtPNWyVsZ6hnUlR0H97O4DSqfzH7fYoSjIZn\n4xep7Ow0yO29kClx2VbpKJRifPkcwDUJojqP1BcA7CbVpbddNAwk4ohqEmVFSBoU\ndjY8ew3P5UOFZzepBwFdoOZhqtzSXWLs3ApOITCWJuHHWwrxRqSXmPLqy7e5knjX\npQIDAQAB\n-----END PUBLIC KEY-----";
    std::string privateKey1 = "-----BEGIN PRIVATE KEY-----\nMIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDRysa/pXUvcEB5\nf54HLvke5102f+MEQ2Z0+Zpmy5TNh39F8Cyp3lq6kUdrldKLEIHGwu9nM9zRQfPP\nbcLtD2ec0yt/F02xUHfpE70tRdVSW5jlWleglmLMWmpJI9At3OkL/gRN92pVjmZ4\nV7lDj39KftUjeIFyh6FQ4trittgMpuht7Vgm081bJWxnqGdSVHQf3s7gNKp/Mft9\nihKMhmfjF6ns7DTI7b2QKXHZVukolGJ8+RzANQmiOo/UFwDsJtWlt100DCTiiGoS\nZUVIGhR2Njx7Dc/lQ4VnN6kHAV2g5mGq3NJdYuzcCk4hMJYm4cdbCvFGpJeY8urL\nt7mSeNelAgMBAAECggEALLNUHcmXaoA0fK7gcQ9lLVyG0/Hz4RirYAk/COAf2Jsi\nVziRi7BBDLefzCCkN9VQkZU/hXHbfwradDwi6Hf7z8J+5hmFCF7o7dSy3k6e4Wl6\n7oONYD4q1vf85ZCn7t2/GjsJl8M7+PbahpHKe8a7jJfxuhkXG9wiVyW/Fcd3yVn0\nMiewcJD6/VQudy4yCRWPv7Y05/UhGvd1d77cY7aWy8RIg6XeaamU6ZyRnFaWHArx\nxoXyBTY4VVcln0woK+Ki01RBtY6kG1jZ46aCIKcTaTdjcVX4LrtMH5Wk88iBblnp\n16iexXsQLAMWdMvQ3yBvOB/R5Wp56NfwtAWJc9rxgQKBgQDn/4wwwchkSfV3esAX\nDhDgVR8fuQBqd/p3l1y0I0OvG5smGsJ7iPZUfvJ2OzgFxHWjRl0pwhu9cIISMCUD\njR9BHIZmouMLu3YHu0UUsxCmzExQXFzzJFvHQVCrr3aKV901JNE/gD2Ci90Nzo2h\nvoOd11IIYUi78swalWLoUJ/L7QKBgQDnfxnmyPgh5LC7AVrVvji1gEgRvoXZNz0U\nmLuz3lD5tSugDgf0Efzc8hhYN/dkHOVeUMsqJzu8Leq96EHuQ6AiGlS6141imCGd\nSiOHyeMAZQEVPtuDbEpSOYXQzbnO+s2gEKt3CGDIj588gv5eUVFcB5b+XACSn5dZ\nRPTfdT7zmQKBgAZjvmu1WpyQgOc6hUHdTE+xaHsKMF2+UjKrA42ejwWqn/pLsGGQ\noyAuouTouqFGCOtnS7eTtgngqGRx2QMhBuRXMchv4gr3rceGu99XEzVU2SE5egHk\nVXbGcL9ZxM0IoHoPOogiMw9+ZKc1sibrRVq6jHLYoxUyGbb9uEyns879AoGAJ1io\nC1zPJ9uZ2j8RtFCfjOHf3fw2/cNww0ZuaNT5iGetoYeg/G/uPZN8ZcolZ0OuDIjI\n70I52fMn+d03D4s49XLqQdOPOVnIJNbMETFUPuXr+DN11fGa9DzIrMO6uB5Swsjy\ni8nFwXD/zKYrG9bQcEbt+A+lHUa4z7hzsmNYLskCgYEAovchA3C04pEG8bwEeS7g\n/Rv5gEJUjV4W5ekgAybR8pCxoD3u4usyQ6UVLBALlPERzLJxvqZGtfq2rCJGUvtZ\nhd2y4rFwWBiTzT+/r4Tzl0PJpOqg4iGpR6NWwGsGm712HEG2T0Gvx5Lz8pvIfi3z\nA5iRXfZBuvmTlQg8koHgFac=\n-----END PRIVATE KEY-----";

    std::string publicKey2 = "-----BEGIN PUBLIC KEY-----\nMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAsjqIM5H53RrNJvfuE+cE\nMyDX7F2DLaNN1Z3z3qYMXBJCcA/yQK+8Jc37UeWgtbOUmKiInLmV2UJEr+oG+G2S\nXnSS5DENO4DYryblzJsOtm3+lGY/z846gKnCVADH4CQcMso5SizqvTiMBc/vKUlg\nol9wLo/pRENFmNiiS1RUETHdnoF0ZxpGqFH4XYFtKThokFaeC5IZLVYMuzKDrp/Z\nt7+pY4Y0VwCWOZeiiXYlFkOBAo0g2iL7hT9Kawv1jxLNjsBAhaE3RpRCz3oXmsYy\nngvOl6dwhvDyCB+WnpKIMkkU9/wgcikCPw3CE69BCjSLYTcZoo5m5n2+zEg5e0ZY\n/wIDAQAB\n-----END PUBLIC KEY-----";
    std::string privateKey2 = "-----BEGIN PRIVATE KEY-----\nMIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCyOogzkfndGs0m\n9+4T5wQzINfsXYMto03VnfPepgxcEkJwD/JAr7wlzftR5aC1s5SYqIicuZXZQkSv\n6gb4bZJedJLkMQ07gNivJuXMmw62bf6UZj/PzjqAqcJUAMfgJBwyyjlKLOq9OIwF\nz+8pSWCiX3Auj+lEQ0WY2KJLVFQRMd2egXRnGkaoUfhdgW0pOGiQVp4LkhktVgy7\nMoOun9m3v6ljhjRXAJY5l6KJdiUWQ4ECjSDaIvuFP0prC/WPEs2OwECFoTdGlELP\neheaxjKeC86Xp3CG8PIIH5aekogySRT3/CByKQI/DcITr0EKNIthNxmijmbmfb7M\nSDl7Rlj/AgMBAAECggEAAadJXDEegE6fWJ00ODsMbuz/E9JKRUIelrzAZnBj7Pa+\nZwu+05rNxAwFKN1YgFcKKeBmZA3Utg9HU9p39hYOJZIvSq6p0MOBPx69QwitOkQK\n+JV6QhNHt6nbg6v+LrYnDNGTdny0MM01f1hOf7OOkfMQt13ebCOzoZdg6hH+Naqc\nywC2myiqFmwJwodJ0NQK/k9hemecERfGvy+276Q0q7DRu/K8w98OqMbSTzV7TpdC\n3tc0qsbOuWPqvULqTyYY9jupsRAb8cctl4cCxg/mUI1+wWJ/LKFSQcErjx79sIEa\nUO4XDp3yU9/mQm7+2ZctJtwKjEyl5Tn5bUtovERVAQKBgQDj1ySnyNlGuMefs+m/\nj0W+ONb3vkbQf56k4u7aX1jKp2ptnaCt35tNNan6SFRuknbCG/gBMq0+gd/j2cIJ\n5Cjps/gFfClIz0PQFVlEsZNMAWxMrD4Wxb8fpBbFciOVy1X1jNVTfpAITTWHteuV\ni22BtjNcqAwex1gOsa/k538jjQKBgQDIQaz2xvtsHB5680Gjcm2wFn+9IQvtb3d5\nH6Yi2A9ZNnFD3vYRKFACWTMYPdnqseBXR2R9IqDlWdbszuhqzG02PeM1rcNL/LyA\nvu1UJLRtKH4dVqx7znWorLJehk6bCSR2JD5ijHAoVfWlLGTQTRUKfrkKTw7Uzlwm\nTnQbc1QluwKBgQCreOC6cfusMSbz96iFJePcXNTUkVykUFfqSmxu5vFhW4xKwSYL\nlc1A15F8rvD0YsCEKB6HcEdYUtBYoCtb3F46PNr97cr4ZBzqPxb3DxoHSs7iCYOV\nCfBkdM86fENx2h1wdzSZ6RenV8xgvbZ2zv90btbK9iJhC7AnJu11PhC+hQKBgCm+\n9y2ioXsSCZCb8Uz/Z7pTlmF46CGhIQjQ/jM5U0nHvajma+l7u+IhcjNVgX4Zgqjv\nKxWjCGOHbPSE1ZKd9w5drGXeSV4n26wDITpvRGWVEWVQUjik+4YkKjLmULClIUK3\nn4GvwRnHgaPjM0jxKLe9Xxm1DWRzeGZL6IxoaFxNAoGAQfWQMUvVmFRrmfBcSEz9\n5wEyJd0tOE4A/3VxuyZcXTKyymReIsxRVI2UN/48rI7ZgbcTWqY7c0OnV/Ta3jYL\nLXCgJCfghYItdDgEP+BITIUWTwhdLZu0nM4RyRbZZKFu/LQICLJuQxkCcZqIQmTB\nK4/B+xbDr+gmP96F6nNvlL4=\n-----END PRIVATE KEY-----";

    CryptoKernel::Blockchain blockchain;

    std::vector<CryptoKernel::Blockchain::output> inputs;
    inputs = blockchain.getUnspentOutputs(publicKey1);

    CryptoKernel::Crypto crypto(false);
    crypto.setPrivateKey(privateKey1);

    double amount = 284.36884;
    std::vector<CryptoKernel::Blockchain::output> toSpend;

    double accumulator = 0;
    std::vector<CryptoKernel::Blockchain::output>::iterator it;
    for(it = inputs.begin(); it < inputs.end(); it++)
    {
        if(accumulator < amount)
        {
            accumulator += (*it).value;
            (*it).signature = crypto.sign((*it).id);
            toSpend.push_back(*it);
        }
        else
        {
            break;
        }
    }

    CryptoKernel::Blockchain::output toThem;
    toThem.value = amount;
    toThem.publicKey = publicKey2;

    time_t t = std::time(0);
    uint64_t now = static_cast<uint64_t> (t);
    toThem.nonce = now;
    toThem.id = blockchain.calculateOutputId(toThem);

    CryptoKernel::Blockchain::output change;
    change.value = accumulator - amount;
    change.publicKey = publicKey1;
    change.nonce = now;
    change.id = blockchain.calculateOutputId(change);

    CryptoKernel::Blockchain::transaction tx;
    tx.inputs = toSpend;
    tx.outputs.push_back(toThem);
    tx.outputs.push_back(change);
    tx.timestamp = now;
    tx.id = blockchain.calculateTransactionId(tx);

    blockchain.submitTransaction(tx);

    while(true)
    {
        CryptoKernel::Blockchain::block Block;
        Block = blockchain.generateMiningBlock(publicKey1);
        Block.nonce = 0;

        std::string data = CryptoKernel::Storage::toString(blockchain.blockToJson(Block));
        std::cout << data << std::endl;

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
        //std::cout << blockchain.getBalance(publicKey1) << std::endl;
    }

    return 0;
}

