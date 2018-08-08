#include <queue>
#include "merkletree.h"
#include "crypto.h"


CryptoKernel::MerkleNode::MerkleNode() {
    leaf = true;
    ancestor = nullptr;
}

CryptoKernel::MerkleNode::MerkleNode(const BigNum& left, const BigNum& right) {
    leaf = true;
    
    leftVal = left;
    rightVal = right;
    ancestor = nullptr;

    root = calcRoot(leftVal.toString(), rightVal.toString());
}

CryptoKernel::MerkleNode::MerkleNode(const BigNum& left) : MerkleNode(left, left) {

}

CryptoKernel::MerkleNode::MerkleNode(const std::shared_ptr<MerkleNode> left,
                                     const std::shared_ptr<MerkleNode> right) {
    leaf = false;
    
    leftNode = left;
    rightNode = right;
    leftNode->ancestor = this;
    rightNode->ancestor = this;
    ancestor = nullptr;

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

std::shared_ptr<CryptoKernel::MerkleNode> CryptoKernel::MerkleNode::getLeftNode() {
    return leftNode;
}

std::shared_ptr<CryptoKernel::MerkleNode> CryptoKernel::MerkleNode::getRightNode() {
    return rightNode;
}

CryptoKernel::MerkleNode* CryptoKernel::MerkleNode::getAncestor() const {
    return ancestor;
}

CryptoKernel::BigNum CryptoKernel::MerkleNode::calcRoot(const std::string& left,
                                                        const std::string& right) {
    CryptoKernel::Crypto crypto;
    return CryptoKernel::BigNum(crypto.sha256(left + right));
}

CryptoKernel::MerkleRootNode::MerkleRootNode(const BigNum& merkleRoot) {
    leaf = true;
    root = merkleRoot;
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

const CryptoKernel::MerkleNode* CryptoKernel::MerkleNode::findDescendant(const BigNum& needle) const{
    if(leftVal == needle || rightVal == needle) {
        return this;
    }
    else if(leaf) { 
        return nullptr; 
    } else {
        const CryptoKernel::MerkleNode* findLeft = leftNode->findDescendant(needle);
        if(findLeft != nullptr) return findLeft;
        const CryptoKernel::MerkleNode* findRight = rightNode->findDescendant(needle);
        return findRight;
    }
}

std::shared_ptr<CryptoKernel::MerkleProof> CryptoKernel::MerkleNode::makeProof(BigNum proof) {
    const CryptoKernel::MerkleNode* proofNode = findDescendant(proof);
    if(proofNode == nullptr) {
        throw CryptoKernel::Blockchain::NotFoundException("Tree node " + proof.toString());
    }
    std::shared_ptr<CryptoKernel::MerkleProof> result = std::make_shared<CryptoKernel::MerkleProof>();
    result->leaves.push_back(proof);
    int level = 0;

    while(proofNode) {
        result->leaves.push_back((proofNode->getLeftVal() == proof) ? proofNode->getRightVal() : proofNode->getLeftVal());
        if(!proofNode->getAncestor()) break; 
        if(!(proofNode->getLeftVal() == proof)) result->positionInTotalSet += (1 << level);
        proof = proofNode->getMerkleRoot();
        proofNode = proofNode->getAncestor(); 
        level++;
    }

    return result;
}

std::shared_ptr<CryptoKernel::MerkleNode> CryptoKernel::MerkleNode::makeMerkleTreeFromProof(std::shared_ptr<CryptoKernel::MerkleProof> proof) {
    const BigNum& provingElement = proof->leaves.at(0);
    // If the set size is just 1, it was a merkle tree of 1 node
    if(proof->leaves.size() == 1) return std::make_shared<MerkleRootNode>(provingElement);

    const BigNum& firstSibling = proof->leaves.at(1);

    std::shared_ptr<CryptoKernel::MerkleNode> result;
    
    result = std::make_shared<CryptoKernel::MerkleNode>(
            (proof->positionInTotalSet % 2 == 0) ? provingElement : firstSibling,
            (proof->positionInTotalSet % 2 == 0) ? firstSibling : provingElement
        );      
  
    if(proof->leaves.size() == 2) return result;    

    int positionInLayer = (proof->positionInTotalSet/2);
    std::set<BigNum>::iterator it;
    for (int i = 2; i < proof->leaves.size(); i++)
	{
        const BigNum& siblingValue = proof->leaves.at(i);

        std::shared_ptr<CryptoKernel::MerkleNode> sibling = std::make_shared<CryptoKernel::MerkleRootNode>(siblingValue);

        result = std::make_shared<CryptoKernel::MerkleNode>(
                (positionInLayer % 2 == 0) ? result : sibling,
                (positionInLayer % 2 == 0) ? sibling : result
            );

        positionInLayer /= 2;
	}
 
    return result;
}

Json::Value CryptoKernel::MerkleProof::toJson() const {
    Json::Value result;

    result["position"] = positionInTotalSet;
    for(const BigNum& leaf : leaves) {
        result["leaves"].append(leaf.toString());
    }
    return result;
}

CryptoKernel::MerkleProof::MerkleProof() {
    positionInTotalSet = 0;
    leaves = {};
} 

CryptoKernel::MerkleProof::MerkleProof(const Json::Value& jsonProof) {
    if(jsonProof["position"].isNull()){
        throw CryptoKernel::Blockchain::InvalidElementException("Merkle proof JSON is malformed: Mandatory element 'position' is not found");
    }
    if(jsonProof["leaves"].isNull()){
        throw CryptoKernel::Blockchain::InvalidElementException("Merkle proof JSON is malformed: Mandatory element 'leaves' is not found");
    }
    if(jsonProof["leaves"].empty()){
        throw CryptoKernel::Blockchain::InvalidElementException("Merkle proof JSON is malformed: Mandatory element 'leaves' is empty");
    }
    
    try {
        
        leaves = {};
        positionInTotalSet = jsonProof["position"].asInt();
        for(const Json::Value leaf : jsonProof["leaves"]) {
            leaves.push_back(CryptoKernel::BigNum(leaf.asString()));
        }
    } catch(const Json::Exception& e) {
        throw CryptoKernel::Blockchain::InvalidElementException("Merkle proof JSON is malformed");
    }
}