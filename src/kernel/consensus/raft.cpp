#include "raft.h"

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
    if(blockData.leader == currentLeader) {

    } else {
        return false;
    }
    // Check if block is from current leader
        // If so, check if block has majority of votes
            // If so, return true hence updating the blockchain
            // Otherwise, return false and potentially vote for the block
}
