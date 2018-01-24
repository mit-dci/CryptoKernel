#include <future>

#include "raft.h"
#include "crypto.h"

CryptoKernel::Consensus::Raft::Raft(const std::set<std::string> validators,
                                    const uint64_t heartbeatTimeout,
                                    const bool validator,
                                    const std::string privKey) {
    this->validators = validators;
    this->heartbeatTimeout = heartbeatTimeout;
    currentState = nodeState::follower;
    voted = false;
    this->validator = validator;
    this->privKey = privKey;
    timeoutGate.reset(new Gate());

    running = true;
    timeoutThread.reset(new std::thread(&CryptoKernel::Consensus::Raft::timeoutFunc, this));
}

CryptoKernel::Consensus::Raft::~Raft() {
    running = false;
    timeoutThread->join();

}

bool CryptoKernel::Consensus::Raft::isBlockBetter(const CryptoKernel::Blockchain::block block, const CryptoKernel::Blockchain::block tip) {
    return false;
}

std::string CryptoKernel::Consensus::Raft::serializeConsensusData(const CryptoKernel::Blockchain::block block) {
    const consensusData blockData = getConsensusData(block);
    return std::to_string(blockData.term);
}

Json::Value CryptoKernel::Consensus::Raft::consensusDataToJson(const consensusData data) {
    Json::Value returning;
    returning["term"] = data.term;
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
    const auto jsonData = block.getConsensusData();
    data.term = jsonData["term"].asUInt64();
    for(unsigned int i = 0; i < jsonData["votes"].size(); i++) {
        data.votes.push_back(std::pair<std::string, std::string>(jsonData["votes"][i]["publicKey"].asString(), jsonData["votes"][i]["signature"].asString()));
    }
    return data;
}

bool CryptoKernel::Consensus::Raft::checkConsensusRules(const CryptoKernel::Blockchain::block block, const CryptoKernel::Blockchain::block previousBlock) {
    const consensusData blockData = getConsensusData(block);
    // Check if block is from current term
    if(electionTerm == blockData.term) {
        // If so, check if block has majority of votes
        std::set<std::string> blockVoters;
        for(const auto vote : blockData.votes) {
            if(validators.find(vote.first) != validators.end()) {
                if(blockVoters.find(vote.first) != blockVoters.end()) {
                    return false;
                } else {
                    CryptoKernel::Crypto crypto;
                    if(!crypto.setPublicKey(vote.first)) {
                        return false;
                    }

                    if(!crypto.verify(block.getId().toString(), vote.second)) {
                        return false;
                    }
                    blockVoters.insert(vote.first);
                }
            } else {
                return false;
            }
        }

        if(blockVoters.size() > 0.5 * validators.size()) {
            return true;
        } else {
            /* The block was otherwise valid but doesn't have enough votes yet

               If we haven't signed the block yet, sign it and broadcast a block
               sign transaction. Also rebroadcast the block so other verifiers
               can see it.
            */

            // Only attempt to vote if we haven't already voted this election term
            // and if we are a validator node
            if(!voted && validator) {
                CryptoKernel::Crypto crypto;
                crypto.setPrivateKey(privKey);

                const auto sig = crypto.sign(block.getId().toString());
                auto newBlockData = blockData;
                newBlockData.votes.push_back(std::make_pair(crypto.getPublicKey(), sig));

                auto newBlock = block;
                newBlock.setConsensusData(consensusDataToJson(newBlockData));

                voted = true;

                // TODO: broadcast voted tx


            }
        }
    }

    return false;
}

bool CryptoKernel::Consensus::Raft::submitBlock(const CryptoKernel::Blockchain::block block) {
    timeoutGate->notify();
    return true;
}

void CryptoKernel::Consensus::Raft::bumpTerm() {
    electionTerm++;
    voted = false;
}

void CryptoKernel::Consensus::Raft::timeoutFunc() {
    while(running) {
        auto timeoutFuture = std::async(std::launch::async, [this](){
            timeoutGate->wait();
        });

        timeoutFuture.wait_for(std::chrono::milliseconds(heartbeatTimeout));
        bumpTerm();
    }
}
