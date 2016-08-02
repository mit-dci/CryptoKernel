#include <iostream>

#include "crypto.h"

int main()
{
    //Crypto Tests

    //Test initialisation
    CryptoKernel::Crypto crypto(true);
    if(!crypto.getStatus())
    {
        std::cout << "Failed to initialise crypto module.";
        return 0;
    }
    else
    {
        std::cout << "Crypto module successfully initialised.\n";
    }

    //Test key pair generation
    std::string privateKey = crypto.getPrivateKey();
    if(privateKey == "")
    {
        std::cout << "Failed to generate a private key.";
        return 0;
    }
    else
    {
        std::cout << "Successfully generated a private key.\n";
    }

    std::string publicKey = crypto.getPublicKey();
    if(publicKey == "")
    {
        std::cout << "Failed to generate a public key.";
        return 0;
    }
    else
    {
        std::cout << "Successfully generated a public key.\n";
    }

    //Test encryption and decryption
    std::string plainText = "This is a test.";

    //Test signing and verifying
    std::string signature = crypto.sign(plainText);
    if(signature != "")
    {
        if(crypto.verify(plainText, signature))
        {
            std::cout << "Successfully signed and verified message.\n";
        }
        else
        {
            std::cout << "Failed to sign or verify message correctly.";
            return 0;
        }
    }
    else
    {
        std::cout << "Failed to sign message.";
        return 0;
    }

    return 0;
}
