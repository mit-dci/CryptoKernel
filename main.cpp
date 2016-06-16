#include <iostream>

#include "crypto.h"

int main()
{
    //Crypto Tests

    //Test initialisation
    CryptoKernel::Crypto *crypto = new CryptoKernel::Crypto(true);
    if(!crypto->getStatus())
    {
        std::cout << "Failed to initialise crypto module.";
        delete crypto;
        return 0;
    }
    else
    {
        std::cout << "Crypto module successfully initialised.\n";
    }

    //Test key pair generation
    std::string privateKey = crypto->getPrivateKey();
    if(privateKey == "")
    {
        std::cout << "Failed to generate a private key.";
        delete crypto;
        return 0;
    }
    else
    {
        std::cout << "Successfully generated a private key.\n";
    }

    std::string publicKey = crypto->getPublicKey();
    if(publicKey == "")
    {
        std::cout << "Failed to generate a public key.";
        delete crypto;
        return 0;
    }
    else
    {
        std::cout << "Successfully generated a public key.\n";
    }

    //Test encryption and decryption
    std::string plainText = "This is a test.";
    std::string cipherText = crypto->encrypt(plainText);
    if(cipherText == "")
    {
        std::cout << "Failed to encrypt string.";
        delete crypto;
        return 0;
    }
    else
    {
        std::string newPlainText = crypto->decrypt(cipherText);
        if(newPlainText != plainText)
        {
            std::cout << "Failed to encrypt or decrypt string.";
            delete crypto;
            return 0;
        }
        else
        {
            std::cout << "Encrypted and decrypted string successfully.\n";
        }
    }

    //Test signing and verifying
    std::string signature = crypto->sign(plainText);
    if(signature != "")
    {
        if(crypto->verify(plainText, signature))
        {
            std::cout << "Successfully signed and verified message.\n";
        }
        else
        {
            std::cout << "Failed to sign or verify message correctly.";
            delete crypto;
            return 0;
        }
    }
    else
    {
        std::cout << "Failed to sign message.";
        delete crypto;
        return 0;
    }

    delete crypto;
    crypto = NULL;

    //Test encryption, decryption, signing and verifying across instances
    crypto = new CryptoKernel::Crypto();

    if(!crypto->setPublicKey(publicKey))
    {
        std::cout << "Failed to set public key.";
        delete crypto;
        return 0;
    }
    else
    {
        cipherText = crypto->encrypt(plainText);
        std::string ek = crypto->getEk();
        if(cipherText != "")
        {
            CryptoKernel::Crypto *crypto2 = new CryptoKernel::Crypto(true);
            std::string publicKey2 = crypto2->getPublicKey();
            signature = crypto2->sign(plainText);
            delete crypto2;
            crypto2 = NULL;

            crypto->setPublicKey(publicKey2);
            if(crypto->verify(plainText, signature))
            {
                std::cout << "Successfully signed and verified a message across instances of crypto.\n";
                if(crypto->setPrivateKey(privateKey))
                {
                    if(crypto->setEk(ek))
                    {
                        if(crypto->decrypt(cipherText) == plainText)
                        {
                            std::cout << "Successfully encrypted and decrypted a string across instances of crypto.\n";
                        }
                        else
                        {
                            std::cout << "Failed to encrypt and decrypt string across instances of crypto.";
                            delete crypto;
                            return 0;
                        }
                    }
                    else
                    {
                        std::cout << "Failed to set encrypted key.";
                        delete crypto;
                        return 0;
                    }
                }
                else
                {
                    std::cout << "Failed to set private key.";
                    delete crypto;
                    return 0;
                }
            }
            else
            {
                std::cout << "Failed to sign or verify message across instances of crypto.";
                delete crypto;
                return 0;
            }
        }
        else
        {
            std::cout << "Failed to encrypt plain text.";
            delete crypto;
            return 0;
        }
    }

    delete crypto;

    return 0;
}

