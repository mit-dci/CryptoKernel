#ifndef RAFT_H_INCLUDED
#define RAFT_H_INCLUDED

#include "blockchain.h"
#include "gate.h"

namespace CryptoKernel {
    class Consensus::Raft : public Consensus {
        public:
            Raft(const std::set<std::string> validators, const uint64_t heartbeatTimeout,
                 const bool validator, const std::string privKey);

            ~Raft();

            /**
            * In Raft, this should always return false. Blocks are only ever accepted if they are from
            * the leader and have a majority of votes from other nodes, so there is no chance of forking.
            */
            bool isBlockBetter(const CryptoKernel::Blockchain::block block, const CryptoKernel::Blockchain::block tip);

            std::string serializeConsensusData(const CryptoKernel::Blockchain::block block);

            bool checkConsensusRules(const CryptoKernel::Blockchain::block block, const CryptoKernel::Blockchain::block previousBlock);

            bool submitBlock(const CryptoKernel::Blockchain::block block);
        private:
            uint64_t heartbeatTimeout;
            bool validator;
            std::string privKey;

            std::set<std::string> validators;

            enum nodeState {
                follower,
                candidate,
                leader
            };

            struct consensusData {
                std::vector<std::pair<std::string, std::string>> votes;
                uint64_t term;
            };

            uint64_t electionTerm;
            bool voted;
            bool running;

            nodeState currentState;
            Json::Value consensusDataToJson(const consensusData data);
            consensusData getConsensusData(const CryptoKernel::Blockchain::block block);

            std::unique_ptr<Gate> timeoutGate;
            std::unique_ptr<std::thread> timeoutThread;
            void bumpTerm();
            void timeoutFunc();
    };
}

#endif // RAFT_H_INCLUDED
