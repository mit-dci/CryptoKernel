#ifndef MERKLETREE_H_INCLUDED
#define MERKLETREE_H_INCLUDED

#include <set>
#include <memory>

#include <json/writer.h>
#include <json/reader.h>

#include "ckmath.h"
#include "blockchain.h"

namespace CryptoKernel {
    class MerkleProof {
        public:
            MerkleProof();
            MerkleProof(const Json::Value& json);
            int positionInTotalSet;
            std::vector<BigNum> leaves;
            Json::Value toJson() const;
    };

    class MerkleNode {
        public:
            MerkleNode();
            
            MerkleNode(const std::shared_ptr<MerkleNode> left, 
                       const std::shared_ptr<MerkleNode> right);
            
            MerkleNode(const std::shared_ptr<MerkleNode> left);
            
            MerkleNode(const BigNum& left, const BigNum& right);
            
            MerkleNode(const BigNum& left);
            
            static std::shared_ptr<MerkleNode> makeMerkleTree(const std::set<BigNum>& leaves);
            static std::shared_ptr<CryptoKernel::MerkleNode> makeMerkleTreeFromProof(std::shared_ptr<CryptoKernel::MerkleProof> proof);
            std::shared_ptr<CryptoKernel::MerkleProof> makeProof(BigNum proofValue);
            BigNum getMerkleRoot() const;
            
            BigNum getLeftVal() const;
            BigNum getRightVal() const;
            std::shared_ptr<MerkleNode>  getLeftNode();
            std::shared_ptr<MerkleNode>  getRightNode();
            MerkleNode* getAncestor() const;

        private:
            MerkleNode* ancestor;
            
            std::shared_ptr<MerkleNode> leftNode;
            std::shared_ptr<MerkleNode> rightNode;

            BigNum leftVal;
            BigNum rightVal;
                        
            const CryptoKernel::MerkleNode* findDescendant(const BigNum& needle) const;
            static BigNum calcRoot(const std::string& left, const std::string& right);

        protected:
            bool leaf;
            BigNum root;
    };

    class MerkleRootNode : public MerkleNode {
        public:
            MerkleRootNode(const BigNum& merkleRoot);
    };

    
}

#endif // MERKLETREE_N_H_INCLUDED