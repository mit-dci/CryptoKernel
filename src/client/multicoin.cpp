#include "multicoin.h"

#include "consensus/PoW.h"

CryptoKernel::MulticoinLoader::MulticoinLoader(const std::string& configFile,
                                               Log* log) {
    this->log = log;

    std::ifstream t(configFile);
    if(!t.is_open()) {
        throw std::runtime_error("Could not open multicoin config file");
    }

    const std::string buffer((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
    const Json::Value config = CryptoKernel::Storage::toJson(buffer);

    t.close();

    for(const auto& coin : config["coins"]) {
        Coin* newCoin = new Coin;
        newCoin->name = coin["name"].asString();

        auto subsidyFunc = getSubsidyFunc(coin["subsidy"].asString());
        auto coinbaseOwnerFunc = [](const std::string& publicKey) {
                                      return publicKey;
                                  };

        newCoin->blockchain.reset(new DynamicBlockchain(log,
                                                        coin["blockdb"].asString(),
                                                        coinbaseOwnerFunc,
                                                        subsidyFunc));

        newCoin->consensusAlgo = getConsensusAlgo(coin["consensus"]["type"].asString(),
                                                  coin["consensus"]["params"],
                                                  newCoin->blockchain.get());

        newCoin->blockchain->loadChain(newCoin->consensusAlgo.get(),
                                      coin["genesisblock"].asString());

        newCoin->network.reset(new Network(log, newCoin->blockchain.get(),
                                           coin["port"].asUInt(),
                                           coin["peerdb"].asString()));

        if(!coin["walletdb"].empty()) {
            newCoin->wallet.reset(new Wallet(newCoin->blockchain.get(),
                                            newCoin->network.get(),
                                            log,
                                            coin["walletdb"].asString()));
        }

        coins.push_back(std::unique_ptr<Coin>(newCoin));
    }
}

CryptoKernel::MulticoinLoader::~MulticoinLoader() {

}

std::function<uint64_t(const uint64_t)> CryptoKernel::MulticoinLoader::getSubsidyFunc(
                                  const std::string& name) const {
    if(name == "k320") {
        return [](const uint64_t height) {
            return 0;
        };
    } else {
        throw std::runtime_error("Unknown subsidy function " + name);
    }
}

std::unique_ptr<CryptoKernel::Consensus> CryptoKernel::MulticoinLoader::getConsensusAlgo(
                                         const std::string& name,
                                         const Json::Value& params,
                                         Blockchain* blockchain) {
    if(name == "kgw_lyra2rev2") {
        return std::unique_ptr<CryptoKernel::Consensus>(
               new Consensus::PoW::KGW_LYRA2REV2(params["blocktime"].asUInt64(),
                                                 blockchain));
    } else {
        throw std::runtime_error("Unknown consensus algorithm " + name);
    }
}

CryptoKernel::MulticoinLoader::
DynamicBlockchain::DynamicBlockchain(Log* GlobalLog,
                                     const std::string& dbDir,
                                     std::function<std::string(const std::string&)> getCoinbaseOwnerFunc,
                                     std::function<uint64_t(const uint64_t)> getBlockRewardFunc) :
CryptoKernel::Blockchain(GlobalLog, dbDir) {
    this->getCoinbaseOwnerFunc = getCoinbaseOwnerFunc;
    this->getBlockRewardFunc = getBlockRewardFunc;
}

std::string CryptoKernel::MulticoinLoader::
DynamicBlockchain::getCoinbaseOwner(const std::string& publicKey) {
    return getCoinbaseOwnerFunc(publicKey);
}

uint64_t CryptoKernel::MulticoinLoader::
DynamicBlockchain::getBlockReward(const uint64_t height) {
    return getBlockRewardFunc(height);
}
