#ifndef MERKLETREE_H_INCLUDED
#define MERKLETREE_H_INCLUDED

#include <set>
#include <memory>

#include "ckmath.h"

namespace CryptoKernel {
    class MerkleNode {
        public:
            MerkleNode(const std::shared_ptr<MerkleNode> left, 
                       const std::shared_ptr<MerkleNode> right);
            
            MerkleNode(const std::shared_ptr<MerkleNode> left);
            
            MerkleNode(const BigNum& left, const BigNum& right);
            
            MerkleNode(const BigNum& left);
            
            static std::shared_ptr<MerkleNode> makeMerkleTree(const std::set<BigNum>& leaves);
        
            BigNum getMerkleRoot() const;
            
            BigNum getLeftVal() const;
            BigNum getRightVal() const;
            
        private:
            bool leaf;
            
            std::shared_ptr<MerkleNode> leftNode;
            std::shared_ptr<MerkleNode> rightNode;
            
            BigNum leftVal;
            BigNum rightVal;
            
            BigNum root;
            
            static BigNum calcRoot(const std::string& left, const std::string& right);
    };
}

#endif // MERKLETREE_N_H_INCLUDED