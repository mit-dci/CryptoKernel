/*  CryptoKernel - A library for creating blockchain based digital currency
    Copyright (C) 2016  James Lovejoy

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef BLOCKCHAIN_H_INCLUDED
#define BLOCKCHAIN_H_INCLUDED

#include <vector>
#include <set>
#include <memory>
#include <map>

#include "storage.h"
#include "log.h"
#include "ckmath.h"

namespace CryptoKernel {
class Consensus;
class Blockchain {
public:
    Blockchain(CryptoKernel::Log* GlobalLog,
               const std::string& dbDir);
    virtual ~Blockchain();

    class InvalidElementException : public std::exception {
    public:
        InvalidElementException(const std::string& message) {
            this->message = message;
        }

        const char* what() {
            return message.c_str();
        }
    private:
        std::string message;
    };

    class NotFoundException : public std::exception {
    public:
        NotFoundException(const std::string& message) {
            this->message = message;
        }

        const char* what() {
            return message.c_str();
        }
    private:
        std::string message;
    };

    class output {
    public:
        output(const uint64_t value, const uint64_t nonce, const Json::Value& data);
        output(const Json::Value& jsonOutput);

        Json::Value toJson() const;

        uint64_t getValue() const;
        uint64_t getNonce() const;
        Json::Value getData() const;

        BigNum getId() const;

        bool operator<(const output& rhs) const;

    private:
        void checkRep();

        BigNum calculateId();

        uint64_t value;
        uint64_t nonce;
        Json::Value data;

        BigNum id;
    };

    class input {
    public:
        input(const BigNum& outputId, const Json::Value& data);
        input(const Json::Value& inputJson);

        Json::Value toJson() const;

        Json::Value getData() const;
        BigNum getOutputId() const;
        BigNum getId() const;

        bool operator<(const input& rhs) const;

    private:
        void checkRep();

        BigNum calculateId();

        BigNum outputId;
        Json::Value data;

        BigNum id;

    };

    class transaction {
    public:
        transaction(const std::set<input>& inputs, const std::set<output>& outputs,
                    const uint64_t timestamp, const bool coinbaseTx = false);
        transaction(const Json::Value& jsonTransaction, const bool coinbaseTx = false);

        Json::Value toJson() const;

        BigNum getId() const;
        uint64_t getTimestamp() const;
        std::set<input> getInputs() const;
        std::set<output> getOutputs() const;

        BigNum getOutputSetId() const;

        static BigNum getOutputSetId(const std::set<output>& outputs);

        bool operator<(const transaction& rhs) const;

        unsigned int size() const;

    private:
        void checkRep(const bool coinbaseTx);

        BigNum calculateId();

        std::set<input> inputs;
        std::set<output> outputs;
        uint64_t timestamp;

        BigNum id;

        unsigned int bytes;
    };

    class block {
    public:
        block(const std::set<transaction>& transactions, const transaction& coinbaseTx,
              const BigNum& previousBlockId, const uint64_t timestamp, const Json::Value& consensusData,
              const uint64_t height, const Json::Value data = Json::nullValue);
        block(const Json::Value& jsonBlock);

        Json::Value toJson() const;

        std::set<transaction> getTransactions() const;
        transaction getCoinbaseTx() const;
        BigNum getPreviousBlockId() const;
        uint64_t getTimestamp() const;
        Json::Value getConsensusData() const;
		Json::Value getData() const;
        uint64_t getHeight() const;
		BigNum getTransactionMerkleRoot() const;

        void setConsensusData(const Json::Value& data);

        BigNum getId() const;

    private:
        void checkRep();

        BigNum calculateId();

        std::set<transaction> transactions;
        transaction coinbaseTx;
        BigNum previousBlockId;
        uint64_t timestamp;
        Json::Value consensusData;
		Json::Value data;
        uint64_t height;
		BigNum transactionMerkleRoot;

        BigNum id;
    };

    class dbBlock {
    public:
        dbBlock(const block& compactBlock);
        dbBlock(const block& compactBlock, const uint64_t height);
        dbBlock(const Json::Value& jsonBlock);

        Json::Value toJson() const;

        std::set<BigNum> getTransactions() const;
        BigNum getCoinbaseTx() const;
        BigNum getPreviousBlockId() const;
        uint64_t getTimestamp() const;
        Json::Value getConsensusData() const;
		Json::Value getData() const;
		BigNum getTransactionMerkleRoot() const;

        uint64_t getHeight() const;

        BigNum getId() const;

    private:
        void checkRep();

        BigNum calculateId();

        std::set<BigNum> transactions;
        BigNum coinbaseTx;
        BigNum previousBlockId;
        uint64_t timestamp;
        Json::Value consensusData;
		Json::Value data;
        uint64_t height;
		BigNum transactionMerkleRoot;

        BigNum id;
    };

    class dbInput : public input {
    public:
        dbInput(const input& compactInput);
        dbInput(const Json::Value& inputJson);

        Json::Value toJson() const;
    };

    class dbOutput : public output {
    public:
        dbOutput(const output& compactOutput, const BigNum& creationTx);
        dbOutput(const Json::Value& jsonOutput);

        Json::Value toJson() const;

    private:
        BigNum creationTx;
    };

    class dbTransaction {
    public:
        dbTransaction(const transaction& compactTransaction, const BigNum& confirmingBlock,
                      const bool coinbaseTx = false);
        dbTransaction(const Json::Value& jsonTransaction);

        Json::Value toJson() const;

        BigNum getId() const;
        bool isCoinbaseTx() const;
        uint64_t getTimestamp() const;
        std::set<BigNum> getInputs() const;
        std::set<BigNum> getOutputs() const;

    private:
        void checkRep();

        BigNum calculateId();

        BigNum confirmingBlock;
        bool coinbaseTx;
        uint64_t timestamp;
        std::set<BigNum> inputs;
        std::set<BigNum> outputs;

        BigNum id;
    };

    std::tuple<bool, bool> submitTransaction(const transaction& tx);
    std::tuple<bool, bool> submitBlock(const block& newBlock, bool genesisBlock = false);

    block generateVerifyingBlock(const std::string& publicKey);

    block getBlock(Storage::Transaction* transaction, const std::string& id);
    block getBlockByHeight(Storage::Transaction* transaction, const uint64_t height);

    dbBlock getBlockDB(Storage::Transaction* transaction, const std::string& id,
                       const bool mainChain = false);
    dbBlock getBlockByHeightDB(Storage::Transaction* transaction, const uint64_t height);

    dbBlock getBlockDB(const std::string& id);
    dbBlock getBlockByHeightDB(const uint64_t height);

    block buildBlock(Storage::Transaction* transaction, const dbBlock& dbblock);

    block getBlock(const std::string& id);

    /**
    * Retrieves the block from the current main chain with the given height
    *
    * @param height the height of the block to retrieve
    * @return the block with the given height in the main chain
    * @throw NotFoundException if the block is not found
    */
    block getBlockByHeight(const uint64_t height);

    /**
    * Retrieves the transaction with the given id
    *
    * @param transaction the database transaction this query will be performed on
    * @param id the id of the transaction to get
    * @return the confirmed transaction with the given id
    * @throw NotFoundException if the transaction is not found
    */
    transaction getTransaction(Storage::Transaction* transaction, const std::string& id);

    /**
    * Retrieves the transaction with the given id
    *
    * @param id the id of the transaction to get
    * @return the confirmed transaction with the given id
    * @throw NotFoundException if the transaction is not found
    */
    transaction getTransaction(const std::string& id);

    dbTransaction getTransactionDB(Storage::Transaction* transaction, const std::string& id);

    /**
    * Retrieves the output, spent or unspent, that is associated
    * with the given id
    *
    * @param id the id of the output to get
    * @return the output with the given id in the main chain
    * @throw NotFoundException if the output is not found
    */
    output getOutput(const std::string& id);

    output getOutput(Storage::Transaction* dbTx, const std::string& id);

    dbOutput getOutputDB(Storage::Transaction* dbTx, const std::string& id);

    input getInput(Storage::Transaction* dbTx, const std::string& id);

    std::set<dbOutput> getUnspentOutputs(const std::string& publicKey);

    std::set<dbOutput> getSpentOutputs(const std::string& publicKey);

    std::set<transaction> getUnconfirmedTransactions();

    /**
    * Loads the chain from disk using the given consensus class
    *
    * @param consensus the consensus method to use with this blockchain
    * @param genesisBlockFile the path to the genesis block JSON file
    * @return true iff the chain loaded successfully
    */
    bool loadChain(Consensus* consensus, const std::string& genesisBlockFile);

    Storage::Transaction* getTxHandle();

    unsigned int mempoolCount() const;
    unsigned int mempoolSize() const;

private:
    std::unique_ptr<Storage::Table> blocks;
    std::unique_ptr<Storage::Table> candidates;
    std::unique_ptr<Storage::Table> transactions;
    std::unique_ptr<Storage::Table> utxos;
    std::unique_ptr<Storage::Table> stxos;
    std::unique_ptr<Storage::Table> inputs;

    std::unique_ptr<Storage> blockdb;
    BigNum genesisBlockId;
    Log *log;

	class Mempool {
		public:
			Mempool();

			bool insert(const transaction& tx);
			void remove(const transaction& tx);
			std::set<transaction> getTransactions() const;
			void rescanMempool(Storage::Transaction* dbTx, Blockchain* blockchain);

            unsigned int count() const;
            unsigned int size() const;

		private:
			std::map<BigNum, transaction> txs;
			std::map<BigNum, BigNum> outputs;
			std::map<BigNum, BigNum> inputs;

            unsigned int bytes;
	};

    Mempool unconfirmedTransactions;
    std::mutex mempoolMutex;

    std::string dbDir;

    std::tuple<bool, bool> verifyTransaction(Storage::Transaction* dbTransaction, const transaction& tx,
                           const bool coinbaseTx = false);
    void confirmTransaction(Storage::Transaction* dbTransaction, const transaction& tx,
                            const BigNum& confirmingBlock, const bool coinbaseTx = false);
    uint64_t getTransactionFee(const transaction& tx);
    uint64_t calculateTransactionFee(Storage::Transaction* dbTx, const transaction& tx);
    bool status;
    void reverseBlock(Storage::Transaction* dbTransaction);
    bool reorgChain(Storage::Transaction* dbTransaction, const BigNum& newTipId);
    virtual uint64_t getBlockReward(const uint64_t height) = 0;
    virtual std::string getCoinbaseOwner(const std::string& publicKey) = 0;
    Consensus* consensus;
    void emptyDB();
    std::tuple<bool, bool> submitTransaction(Storage::Transaction* dbTx, const transaction& tx);
    std::tuple<bool, bool> submitBlock(Storage::Transaction* dbTx, const block& newBlock,
                     bool genesisBlock = false);
    friend class Consensus;
    friend class ContractRunner;
};

/**
* Custom consensus protocol interface.
*
* Extend this class to provide a custom consensus mechanism
* to the blockchain for use in a distributed system.
*/
class Consensus {
public:
    virtual ~Consensus() {};

    /**
    * Pure virtual method that returns true if the given block forms a
    * longer chain than the current chain tip. Internally it is used to
    * determine what is the longest chain. For Proof of Work
    * this method would check if the "total work" of one block is greater
    * than the chain tip, and if so, return true.
    *
    * @param block the block to compare to the current chain tip
    * @param tip the current chain tip block
    * @return true iff block takes precedence over the current chain tip,
    *         otherwise false
    */
    virtual bool isBlockBetter(Storage::Transaction* transaction,
                               const CryptoKernel::Blockchain::block& block,
                               const CryptoKernel::Blockchain::dbBlock& tip) = 0;

    /**
    * Pure virtual method that returns true iff the given block conforms to
    * the consensus rules of the blockchain. For Proof of Work this function
    * would check that the difficulty of the block is correct, the PoW is
    * correct, below the block target and that the total work is correct.
    * This data is stored in the consensusData field of Blockchain::block.
    *
    * @param block the block to check the consensus rules of
    * @return true iff the rules are valid, otherwise false
    */
    virtual bool checkConsensusRules(Storage::Transaction* transaction,
                                     CryptoKernel::Blockchain::block& block,
                                     const CryptoKernel::Blockchain::dbBlock& previousBlock) = 0;

    /**
    * Pure virtual function that generates the consensus data
    * for a block owned by the given public key. In a Proof of
    * Work system this would calculate the block target.
    *
    * @param block the block to generate consensusData for
    * @param publicKey the public key to that will own this block
    * @return the consensusData for the block
    */
    virtual Json::Value generateConsensusData(Storage::Transaction* transaction,
            const CryptoKernel::BigNum& previousBlockId, const std::string& publicKey) = 0;

    /**
    * Callback for custom transaction behavior when when the blockchain needs to check
    * a transaction's validity. Should return true iff the given transaction is valid given
    * the current state of the blockchain according to any custom rules.
    *
    * @param tx the transaction to check the validity of
    * @return true iff the transaction is valid given the current blockchain state
    */
    virtual bool verifyTransaction(Storage::Transaction* transaction,
                                   const CryptoKernel::Blockchain::transaction& tx) = 0;

    /**
    * Callback for custom transaction behavior when the blockchain if confirming a transaction
    * after it has been included in a block, thus updating the blockchain state. Should return
    * true iff the transaction was successfully confirmed according to any custom rules.
    *
    * @param tx the transaction to be confirmed
    * @return true iff the transaction was successfully confirmed given the current blockchain state
    */
    virtual bool confirmTransaction(Storage::Transaction* transaction,
                                    const CryptoKernel::Blockchain::transaction& tx) = 0;

    /**
    * Callback for custom transaction behavior when the blockchain is submitting a transaction
    * according to the custom rules.
    *
    * @param tx the transaction being submitted
    * @return true iff the transaction was successfully submitted given the current blockchain state
    */
    virtual bool submitTransaction(Storage::Transaction* transaction,
                                   const CryptoKernel::Blockchain::transaction& tx) = 0;

    /**
    * Callback for custom block submission behavior when the blockchain is submitting a block.
    * This function is called just before the block is to be committed and the transactions
    * confirmed.
    *
    * @param block the block being submitted
    * @return true iff the block was successfully submitted given the current blockchain state
    */
    virtual bool submitBlock(Storage::Transaction* transaction,
                             const CryptoKernel::Blockchain::block& block) = 0;

    /**
     * Starts the consensus algorithm, e.g. raft voting, mining etc
     */
    virtual void start() = 0;

    class PoW;
    class AVRR;
    class Raft;
    class Regtest;
};
}

#endif // BLOCKCHAIN_H_INCLUDED
