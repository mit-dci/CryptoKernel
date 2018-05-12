#include <queue>

#include "merkletree.h"
#include "crypto.h"

CryptoKernel::MerkleNode::MerkleNode(const BigNum& left, const BigNum& right) {
    leaf = true;
    
    leftVal = left;
    rightVal = right;
    
    root = calcRoot(leftVal.toString(), rightVal.toString());
}

CryptoKernel::MerkleNode::MerkleNode(const BigNum& left) : MerkleNode(left, left) {

}

CryptoKernel::MerkleNode::MerkleNode(const std::shared_ptr<MerkleNode> left,
                                     const std::shared_ptr<MerkleNode> right) {
    leaf = false;
    
    leftNode = left;
    rightNode = right;
    
    root = calcRoot(leftNode->getMerkleRoot().toString(), rightNode->getMerkleRoot().toString());
}

CryptoKernel::MerkleNode::MerkleNode(const std::shared_ptr<MerkleNode> left) 
: MerkleNode(left, left) {
    
}

CryptoKernel::BigNum CryptoKernel::MerkleNode::getMerkleRoot() const {
    return root;
}

CryptoKernel::BigNum CryptoKernel::MerkleNode::getLeftVal() const {
    if(leaf) {
        return leftVal;
    } else {
        return leftNode->getMerkleRoot();
    }
}

CryptoKernel::BigNum CryptoKernel::MerkleNode::getRightVal() const {
    if(leaf) {
        return rightVal;
    } else {
        return rightNode->getMerkleRoot();
    }
}

CryptoKernel::BigNum CryptoKernel::MerkleNode::calcRoot(const std::string& left,
                                                        const std::string& right) {
    CryptoKernel::Crypto crypto;
    return CryptoKernel::BigNum(crypto.sha256(left + right));
}

std::shared_ptr<CryptoKernel::MerkleNode> CryptoKernel::MerkleNode::makeMerkleTree(
                                                    const std::set<BigNum>& leaves) {
    std::vector<std::shared_ptr<MerkleNode>> nodes;
    std::queue<BigNum> leafQueue;
    
    for(const BigNum& leaf : leaves) {
        if(leafQueue.size() < 2) {
            leafQueue.push(leaf);
        } else {
            nodes.push_back(std::make_shared<MerkleNode>(leafQueue.front(), leafQueue.back()));
            leafQueue.pop();
            leafQueue.pop();
            
            leafQueue.push(leaf);
        }
    }
    
    if(leafQueue.size() > 0) {
        nodes.push_back(std::make_shared<MerkleNode>(leafQueue.front(), leafQueue.back()));
    }
    
    while(nodes.size() > 1) {
        std::vector<std::shared_ptr<MerkleNode>> newNodes;
        std::queue<std::shared_ptr<MerkleNode>> nodeQueue;
        for(const auto& node : nodes) {
            if(nodeQueue.size() < 2) {
                nodeQueue.push(node);
            } else {
                newNodes.push_back(std::make_shared<MerkleNode>(nodeQueue.front(), nodeQueue.back()));
                nodeQueue.pop();
                nodeQueue.pop();
                
                nodeQueue.push(node);
            }
        }
        
        if(nodeQueue.size() > 0) {
            newNodes.push_back(std::make_shared<MerkleNode>(nodeQueue.front(), nodeQueue.back()));
        }
        
        nodes = newNodes;
    }
    
    return nodes[0];
}
