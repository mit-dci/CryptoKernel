#include "raft.h"
#include "crypto.h"

CryptoKernel::Consensus::Raft::Raft(const std::set<std::string> verifiers, const uint64_t electionTimeout, const uint64_t heartbeatTimeout) {
    this->verifiers = verifiers;
    this->electionTimeout = electionTimeout;
    this->heartbeatTimeout = heartbeatTimeout;
    currentState = nodeState::follower;
}

bool CryptoKernel::Consensus::Raft::isBlockBetter(const CryptoKernel::Blockchain::block block, const CryptoKernel::Blockchain::block tip) {
    return false;
}

std::string CryptoKernel::Consensus::Raft::serializeConsensusData(const CryptoKernel::Blockchain::block block) {
    const consensusData blockData = getConsensusData(block);
    return blockData.leader + blockData.leaderSignature + std::to_string(blockData.term);
}

Json::Value CryptoKernel::Consensus::Raft::consensusDataToJson(const consensusData data) {
    Json::Value returning;
    returning["leader"] = data.leader;
    returning["leaderSignature"] = data.leaderSignature;
    returning["term"] = static_cast<unsigned long long int>(data.term);
    for(std::pair<std::string, std::string> vote : data.votes) {
        Json::Value voteJson;
        voteJson["publicKey"] = vote.first;
        voteJson["signature"] = vote.second;
        returning["votes"].append(voteJson);
    }
    return returning;
}

CryptoKernel::Consensus::Raft::consensusData CryptoKernel::Consensus::Raft::getConsensusData(const CryptoKernel::Blockchain::block block) {
    consensusData data;
    data.leader = block.consensusData["leader"].asString();
    data.leaderSignature = block.consensusData["leaderSignature"].asString();
    data.term = block.consensusData["term"].asUInt64();
    for(unsigned int i = 0; i < block.consensusData["votes"].size(); i++) {
        data.votes.push_back(std::pair<std::string, std::string>(block.consensusData["votes"][i]["publicKey"].asString(), block.consensusData["votes"][i]["signature"].asString()));
    }
    return data;
}

bool CryptoKernel::Consensus::Raft::checkConsensusRules(const CryptoKernel::Blockchain::block block, const CryptoKernel::Blockchain::block previousBlock) {
    const consensusData blockData = getConsensusData(block);
    // Check if block is from current leader
    if(blockData.leader == currentLeader && electionTerm == blockData.term) {
        // If so, check if block has majority of votes
        std::set<std::string> blockVoters;
        for(const auto vote : blockData.votes) {
            if(verifiers.find(vote.first) != verifiers.end()) {
                if(blockVoters.find(vote.first) != blockVoters.end()) {
                    return false;
                } else {
                    CryptoKernel::Crypto crypto;
                    if(!crypto.setPublicKey(vote.first)) {
                        return false;
                    }

                    if(!crypto.verify(block.id, vote.second)) {
                        return false;
                    }
                    blockVoters.insert(vote.first);
                }
            } else {
                return false;
            }
        }

        if(blockVoters.size() > 0.5 * verifiers.size()) {
            return true;
        } else {
            /* The block was otherwise valid but doesn't have enough votes yet

               If we haven't signed the block yet, sign it and broadcast a block
               sign transaction. Also rebroadcast the block so other verifiers
               can see it.


            */
        }
    } else {
        return false;
    }
}

bool CryptoKernel::Consensus::Raft::submitBlock(const CryptoKernel::Blockchain::block block) {

}
