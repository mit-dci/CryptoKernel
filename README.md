CryptoKernel
============

CryptoKernel is a C++ library intended to help create blockchain-based digital currencies. It contains modules for key-value storage with JSON, peer-to-peer networking, ECDSA key generation, signing and verifying, big number operations, logging and a blockchain class for handling a Bitcoin-style write-only log. Designed to be object-oriented and easy to use, I have created a sample currency that you can use for reference here: https://github.com/metalicjames/CryptoCurrency.

Building on Ubuntu 16.04
------------------------

```
sudo apt-get update
sudo apt-get install git build-essential libssl-dev libjsoncpp-dev libenet-dev libleveldb-dev

wget https://www.openssl.org/source/openssl-1.1.0-pre6.tar.gz
tar -xvzf openssl-1.1.0-pre6.tar.gz
cd openssl-1.1.0-pre6
./config
make
sudo make install

cd ../
git clone https://github.com/metalicjames/CryptoKernel.git
cd CryptoKernel
chmod +x build.sh
./build.sh
sudo cp libCryptoKernel.a /usr/local/lib
sudo mkdir /usr/local/include/cryptokernel
sudo cp *.h /usr/local/include/cryptokernel
```

Usage
-----
```
#include <cmath>

#include <cryptokernel/blockchain.h>
#include <cryptokernel/crypto.h>
#include <cryptokernel/math.h>

class MyBlockchain : public CryptoKernel::Blockchain
{
    public:
        MyBlockchain(CryptoKernel::Log* GlobalLog, const uint64_t blockTime) : CryptoKernel::Blockchain(GlobalLog, blockTime)
        {

        }

    private:
        uint64_t getBlockReward(const uint64_t height)
        {
            if(height > 2)
            {
                return 5000000000 / std::log(height);
            }
            else
            {
                return 5000000000;
            }
        }

        std::string PoWFunction(const std::string inputString)
        {
            CryptoKernel::Crypto crypto;
            return crypto.sha256(inputString);
        }
};

int main()
{
    //Create a log object for our blockchain to log output. Parameters are [filename], printToConsole
    CryptoKernel::Log myLog("myLog.txt", true);
    
    MyBlockchain myChain(&myLog, 150);
    myChain.loadChain();
    
    //Generate a keypair. True as the parameter generates a keypair
    CryptoKernel::Crypto crypto(true);
    std::string publicKey = crypto.getPublicKey();
    std::string privateKey = crypto.getPrivateKey();
    
    //Mine a block
    CryptoKernel::Blockchain::block Block;

    Block = myChain.generateMiningBlock(publicKey);
    Block.nonce = 0;
    do
    {
        Block.nonce += 1;
        Block.PoW = myChain.calculatePoW(Block);
    }
    while(!CryptoKernel::Math::hex_greater(Block.target, Block.PoW));

    CryptoKernel::Blockchain::block previousBlock;
    previousBlock = myChain.getBlock(Block.previousBlockId);
    std::string inverse = CryptoKernel::Math::subtractHex("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", Block.PoW);
    Block.totalWork = CryptoKernel::Math::addHex(inverse, previousBlock.totalWork);

    myChain.submitBlock(Block);
    
    return 0;
}
```

```
g++ main.cpp -o test -std=c++14 -lCryptoKernel -ljsoncpp -lcrypto -lleveldb
```

API Reference
-------------

### Blockchain
``` 
Blockchain(CryptoKernel::Log* GlobalLog, const uint64_t blockTime); 
```
Constructs a blockchain object, requires a pointer to a log object for saving error output for the first argument and the block time in seconds as the second. Also requires a valid genesisblock.txt file containing the genesis block.


```
bool submitTransaction(transaction tx);
```
Add a transaction to the unconfirmed transactions pool ready to be included into a block. Returns true on success and false on failure.


```
bool submitBlock(block newBlock, bool genesisBlock = false);
```
Submit a block to the blockchain. Returns true on success and false on failure.


```
uint64_t getBalance(std::string publicKey);
```
Returns the balance of a public key based on the current state of the blockchain.


```
block generateMiningBlock(std::string publicKey);
```
Returns a block to mine with using the provided public key as the coinbase public key.


```
Json::Value transactionToJson(transaction tx);
```
Converts a transaction to a jsoncpp value.


```
Json::Value outputToJson(output Output);
```
Converts a transaction output to a jsoncpp value.


```
Json::Value blockToJson(block Block);
```
Converts a block to a jsoncpp value.


```
block jsonToBlock(Json::Value Block);
```
Converts a jsoncpp value to a block.


```
transaction jsonToTransaction(Json::Value tx);
```
Converts a jsoncpp value to a transaction.


```
output jsonToOutput(Json::Value Output);
```
Converts a jsoncpp value to an output.


```
std::string calculatePoW(block Block);
```
Calculate the proof of work hash of a given block.


```
block getBlock(std::string id);
```
Returns the block associated with the id given.


```
std::vector<output> getUnspentOutputs(std::string publicKey);
```
Returns a vector of the current unspent outputs associated with the public key given based on the current blockchain state.


```
std::string calculateOutputId(output Output);
```
Calculates the id of a given output.


```
std::string calculateTransactionId(transaction tx);
```
Calculates the id of a given transaction.


```
std::string calculateOutputSetId(std::vector<output> outputs);
```
Calculates the id of a given vector of outputs.


```
std::vector<transaction> getUnconfirmedTransactions();
```
Returns a vector of unconfirmed transactions.


### Crypto
```
Crypto(bool fGenerate = false);
```
Contructs a crypto object. Providing true as the operand generates a new ECDSA keypair.


```
bool getStatus();
```
Returns false if there has been a fatal error or true otherwise.

```
std::string sign(std::string message);
```
Signs the given message and returns the signature.

```
bool verify(std::string message, std::string signature);
```
Verifies the given message given the provided signature.


```
std::string getPublicKey();
```
Returns the current public key being used by the crypto object.


```
std::string getPrivateKey();
```
Returns the current private key being used by the crypto object.


```
bool setPublicKey(std::string publicKey);
```
Sets the public key of the crypto object. Returns true on success and false on failure.


```
bool setPrivateKey(std::string privateKey);
```
Sets the private key of the crypto object. Returns true on success and false on failure.


```
std::string sha256(std::string message);
```
Returns a SHA-256 hash of the given message.


### Math
```
static std::string addHex(std::string first, std::string second);
```
Adds the two provided arbitrarily sized hexadecimal strings and returns the result.


```
static std::string subtractHex(std::string first, std::string second);
```
Subtracts the second hexadecimal number from the first hexadecimal number and returns the result.


```
static bool hex_greater(std::string first, std::string second);
```
Returns true if the first hexadecimal number is larger in value than the second and false otherwise.


```
static std::string divideHex(std::string first, std::string second);
```
Divides the first hex number by the second, ignoring any remainder.


```
static std::string multiplyHex(std::string first, std::string second);
```
Multiplies the two hexadecimal numbers.


### Network
```
Network(Log *GlobalLog);
```
Constructs a peer-to-peer network object. Required a pointer to a log object for logging errors. The peers.txt file should contain a list of IP addresses to connect to separated by new lines.


```
bool getStatus();
```
Returns false if there has been a fatal error or true otherwise.


```
bool connectPeer(std::string peeraddress);
```
Attempts to connect to a peer with the given IPv4 address. Returns true on success and false otherwise.


```
bool sendMessage(std::string message);
```
Attempts to broadcast a message to the peer-to-peer network. Returns true on success and false otherwise.


```
std::string popMessage();
```
Pops a message from the top of the received messages stack and returns it.


```
int getConnections();
```
Returns the number of connected peers.


### Storage
```
Storage(std::string filename);
```
Contructs a storage object using the directory given in the operand. If the directory is not found it will be created.


```
bool store(std::string key, Json::Value value);
```
Stores a jsoncpp value at the provided key. Returns true on success and false on failure.


```
Json::Value get(std::string key);
```
Returns the jsoncpp value associated with the provided key.


```
bool erase(std::string key);
```
Attempts to erase the provided key. Returns true on success and false on failure.


```
bool getStatus();
```
Returns false if there has been a fatal error or true otherwise.


```
Iterator* newIterator();
```
Returns a new pointer to a storage iterator object. Usage:
```
CryptoKernel::Storage::Iterator* it = myStorage.newIterator();

for(it->SeekToFirst(); it->Valid(); it->Next())
{
    Json::Value value = it->value();
    std::string key = it->key();
}

delete it;
```

```
static bool destroy(std::string filename);
```
Destroys the database at the given directory. Returns true on success and false otherwise.


```
static Json::Value toJson(std::string json);
```
Converts a json string to a jsoncpp value.


```
static std::string toString(Json::Value json);
```
Converts a jsoncpp value to a string.