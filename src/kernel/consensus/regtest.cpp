#include <sstream>
#include <math.h>
#include <random>

#include "regtest.h"
#include "../crypto.h"

CryptoKernel::Consensus::Regtest::Regtest(CryptoKernel::Blockchain* blockchain) 
{
	this->blockchain = blockchain;   
}

CryptoKernel::Consensus::Regtest::~Regtest() {}

bool CryptoKernel::Consensus::Regtest::isBlockBetter(Storage::Transaction* transaction, const CryptoKernel::Blockchain::block& block, const CryptoKernel::Blockchain::dbBlock& tip) 
{
	const consensusData blockData = getConsensusData(block);
	return blockData.isBetter;
}

bool CryptoKernel::Consensus::Regtest::checkConsensusRules(Storage::Transaction* transaction, CryptoKernel::Blockchain::block& block, const CryptoKernel::Blockchain::dbBlock& previousBlock)
{
	return true;
}

Json::Value CryptoKernel::Consensus::Regtest::generateConsensusData(Storage::Transaction* transaction, const CryptoKernel::BigNum& previousBlockId, const std::string& publicKey)
{
	return Json::Value();
}

/**
* Has no effect, always returns true
*/
bool CryptoKernel::Consensus::Regtest::verifyTransaction(Storage::Transaction* transaction, 
const CryptoKernel::Blockchain::transaction& tx)
{
	return true;
}

/**
* Has no effect, always returns true
*/
bool CryptoKernel::Consensus::Regtest::confirmTransaction(Storage::Transaction* transaction,
	const CryptoKernel::Blockchain::transaction& tx)
{
	return true;
}

/**
* Has no effect, always returns true
*/
bool CryptoKernel::Consensus::Regtest::submitTransaction(Storage::Transaction* transaction,
	const CryptoKernel::Blockchain::transaction& tx)
{
	return true;
}
/**
* Has no effect, always returns true
*/
bool CryptoKernel::Consensus::Regtest::submitBlock(Storage::Transaction* transaction, const CryptoKernel::Blockchain::block& block)
{
	return true;
}

void CryptoKernel::Consensus::Regtest::mineBlock(const bool isBetter, const std::string &pubKey)
{
	CryptoKernel::Blockchain::block Block = blockchain->generateVerifyingBlock(pubKey);
	Json::Value consensusData = Block.getConsensusData();
	consensusData["isBetter"] = isBetter;
	Block.setConsensusData(consensusData);

    std::random_device r;
    std::default_random_engine generator(r());
    std::uniform_int_distribution<uint64_t> distribution(0, std::numeric_limits<uint64_t>::max());

	auto blockJson = Block.toJson();
	blockJson["coinbaseTx"]["outputs"][0]["nonce"] = distribution(generator);
		
	blockchain->submitBlock(CryptoKernel::Blockchain::block(blockJson));
}

CryptoKernel::Consensus::Regtest::consensusData
CryptoKernel::Consensus::Regtest::getConsensusData(const CryptoKernel::Blockchain::block& block) 
{
	consensusData data;
	const Json::Value consensusJson = block.getConsensusData();
	try {
		data.isBetter = consensusJson["isBetter"].asBool();
	} catch(const Json::Exception& e) {
		throw CryptoKernel::Blockchain::InvalidElementException("Block consensusData JSON is malformed");
	}
	return data;
}

CryptoKernel::Consensus::Regtest::consensusData
CryptoKernel::Consensus::Regtest::getConsensusData(const CryptoKernel::Blockchain::dbBlock& block) 
{
	consensusData data;
	const Json::Value consensusJson = block.getConsensusData();
	try {
		data.isBetter = consensusJson["isBetter"].asBool();
	} catch(const Json::Exception& e) {
		throw CryptoKernel::Blockchain::InvalidElementException("Block consensusData JSON is malformed");
	}
	return data;
}

Json::Value CryptoKernel::Consensus::Regtest::consensusDataToJson(const CryptoKernel::Consensus::Regtest::consensusData& data) 
{
	Json::Value returning;
	returning["isBetter"] = data.isBetter;
	return returning;
}

void CryptoKernel::Consensus::Regtest::start() {}
