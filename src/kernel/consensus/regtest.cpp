#include <sstream>
#include <math.h>

#include "regtest.h"
#include "../crypto.h"

CryptoKernel::Consensus::Regtest::Regtest(CryptoKernel::Blockchain* blockchain) 
{
		this->blockchain = blockchain;   
}

CryptoKernel::Consensus::Regtest::~Regtest() 
{
		running = false;
		minerThread->join();
}

bool CryptoKernel::Consensus::Regtest::isBlockBetter(Storage::Transaction* transaction, const CryptoKernel::Blockchain::block& block, const CryptoKernel::Blockchain::dbBlock& tip) 
{
	const consensusData blockData = getConsensusData(block);
	return blockData.isBetter;
}

bool checkConsensusRules(Storage::Transaction* transaction, const CryptoKernel::Blockchain::block& block, const CryptoKernel::Blockchain::dbBlock& previousBlock)
{
	return true;
}

Json::Value generateConsensusData(Storage::Transaction* transaction, const CryptoKernel::BigNum& previousBlockId, const std::string& publicKey)
{
	return Json::Value();
}

/**
* Has no effect, always returns true
*/
bool verifyTransaction(Storage::Transaction* transaction, 
const CryptoKernel::Blockchain::transaction& tx)
{
	return true;
}

/**
* Has no effect, always returns true
*/
bool confirmTransaction(Storage::Transaction* transaction,
	const CryptoKernel::Blockchain::transaction& tx)
{
	return true;
}

/**
* Has no effect, always returns true
*/
bool submitTransaction(Storage::Transaction* transaction,
	const CryptoKernel::Blockchain::transaction& tx)
{
	return true;
}
/**
* Has no effect, always returns true
*/
bool submitBlock(Storage::Transaction* transaction, const CryptoKernel::Blockchain::block& block)
{
	return true;
}

void mineBlock(const bool isBetter, const std::string &pubKey)
{
	CryptoKernel::Blockchain::block Block = blockchain->generateVerifyingBlock(pubKey);
	Json::Value consensusData = Block.getConsensusData();
	consensusData["isBetter"] = isBetter;
	Block.setConsensusData(consensusData);
	blockchain->submitBlock(Block);
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


