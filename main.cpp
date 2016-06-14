#include "crypto.h"

int main()
{
    CryptoKernel::Crypto *crypto = new CryptoKernel::Crypto(false);
    
    delete crypto;
    
    return 0;
}

