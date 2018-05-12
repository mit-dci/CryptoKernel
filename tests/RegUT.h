#ifndef RegUT_H_INCLUDED
#define RegUT_H_INCLUDED

#include <thread>

#include "../blockchain.h"

namespace CryptoKernel {

class Consensus::RegUT : public Consensus{
public:

	/**
	* Regression Unit Tests for isBlockBetter
	*
	*/
	RegUT(CryptoKernel::Blockchain* blockchain);

	virtual ~RegUT();
	/**
    	* In Regression Unit Test this function checks if the total work of the given
    	* block is greater than the total work of the current tip block.
    	*/
	bool isBlockBetter(Storage::Transaction* transaction, 
		const CryptoKernel::Blockchain::block& block,
		const CryptoKernel::Blockchain::dbBlock& tip);

	std::string serializeConsensusData(const CryptoKernel::Blockchain::block& block);
	
	bool checkConsensusRules(Storage::Transaction* transaction,
                const CryptoKernel::Blockchain::block& block,
                const CryptoKernel::Blockchain::dbBlock& previousBlock);

    	Json::Value generateConsensusData(Storage::Transaction* transaction,
                const CryptoKernel::BigNum& previousBlockId, 
		const std::string& publicKey);

	/**
    	* Has no effect, always returns true
    	*/
    	bool verifyTransaction(Storage::Transaction* transaction, 
		const CryptoKernel::Blockchain::transaction& tx);

	/**
	* Has no effect, always returns true
	*/
	bool confirmTransaction(Storage::Transaction* transaction,
		const CryptoKernel::Blockchain::transaction& tx);

	/**
	* Has no effect, always returns true
	*/
	bool submitTransaction(Storage::Transaction* transaction,
		const CryptoKernel::Blockchain::transaction& tx);
	/**
    	* Has no effect, always returns true
    	*/
	bool submitBlock(Storage::Transaction* transaction, const CryptoKernel::Blockchain::block& block);

	void mineBlock(const bool isBetter, const std::string &pubKey);
	
protected:
	CryptoKernel::Blockchain* blockchain;
	struct consensusData {
		bool isBetter;
	};
	CryptoKernel::Consensus::RegUT::consensusData getConsensusData(const CryptoKernel::Blockchain::block& block);
	CryptoKernel::Consensus::RegUT::consensusData getConsensusData(const CryptoKernel::Blockchain::dbBlock& block);
	Json::Value consensusDataToJson(const consensusData& data);

private:
	
	
};



}
#endif //RegUT_H_INCLUDED
