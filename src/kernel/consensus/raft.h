#ifndef RAFT_H_INCLUDED
#define RAFT_H_INCLUDED

#include "blockchain.h"

namespace CryptoKernel {
    class Consensus::Raft : public Consensus {
        public:
            Raft(const std::set<std::string> verifiers, const uint64_t electionTimeout, const uint64_t heartbeatTimeout);

            /**
            * In Raft, this should always return false. Blocks are only ever accepted if they are from
            * the leader and have a majority of votes from other nodes, so there is no chance of forking.
            */
            bool isBlockBetter(const CryptoKernel::Blockchain::block block, const CryptoKernel::Blockchain::block tip);

            std::string serializeConsensusData(const CryptoKernel::Blockchain::block block);

            bool checkConsensusRules(const CryptoKernel::Blockchain::block block, const CryptoKernel::Blockchain::block previousBlock);

        private:
            uint64_t electionTimeout;
            uint64_t heartbeatTimeout;
            std::set<std::string> verifiers;
            enum nodeState {
                follower,
                candidate,
                leader
            };
            uint64_t electionTerm;
            std::string currentLeader;
            struct consensusData {
                std::string leader;
                std::string leaderSignature;
                std::vector<std::pair<std::string, std::string>> votes;
                uint64_t term;
            };
            nodeState currentState;
            Json::Value consensusDataToJson(const consensusData data);
            consensusData getConsensusData(const CryptoKernel::Blockchain::block block);
    };
}

#endif // RAFT_H_INCLUDED
