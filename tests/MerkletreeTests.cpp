#include "MerkletreeTests.h"

CPPUNIT_TEST_SUITE_REGISTRATION(MerkletreeTest);

MerkletreeTest::MerkletreeTest() {
}

MerkletreeTest::~MerkletreeTest() {
}

void MerkletreeTest::setUp() {
}

void MerkletreeTest::tearDown() {
}

void MerkletreeTest::testGetMerkleRoot() {
    CryptoKernel::BigNum leftVal = CryptoKernel::BigNum("aBc381023c383Def");
    CryptoKernel::BigNum rightVal = CryptoKernel::BigNum("bAc391045cEE3Dfe");
    CryptoKernel::MerkleNode node = CryptoKernel::MerkleNode(leftVal, rightVal);

    const std::string actual = node.getMerkleRoot().toString();
    const std::string expected = "639d30b6811f703aac5a8296e4878e7ab6eeadf9b05e2821390ed0776bdd96be";
    
    CPPUNIT_ASSERT_EQUAL(expected, actual);

    CryptoKernel::BigNum leftVal2 = CryptoKernel::BigNum("cDc381023c383DbE");
    CryptoKernel::BigNum rightVal2 = CryptoKernel::BigNum("cAc391045cEE3DEE");
    CryptoKernel::MerkleNode node2 = CryptoKernel::MerkleNode(leftVal2, rightVal2);

    const std::string actual2 = node2.getMerkleRoot().toString();
    const std::string expected2 = "128520a07c192a97938d9863ef9626b389ebb9a2fcb089ee8ccfc35e1846e997";

    CPPUNIT_ASSERT_EQUAL(expected2, actual2);
}

void MerkletreeTest::testGetLeftVal() {
    CryptoKernel::BigNum leftVal = CryptoKernel::BigNum("aBc381023c383Def");
    CryptoKernel::BigNum rightVal = CryptoKernel::BigNum("bAc391045cEE3Dfe");
    CryptoKernel::MerkleNode node = CryptoKernel::MerkleNode(leftVal, rightVal);

    const std::string actual = node.getLeftVal().toString();
    const std::string expected = leftVal.toString();

    CPPUNIT_ASSERT_EQUAL(expected, actual);
}

void MerkletreeTest::testGetRightVal() {
    CryptoKernel::BigNum leftVal = CryptoKernel::BigNum("aBc381023c383Def");
    CryptoKernel::BigNum rightVal = CryptoKernel::BigNum("bAc391045cEE3Dfe");
    CryptoKernel::MerkleNode node = CryptoKernel::MerkleNode(leftVal, rightVal);

    const std::string actual = node.getRightVal().toString();
    const std::string expected = rightVal.toString();
    
    CPPUNIT_ASSERT_EQUAL(expected, actual);
}

void MerkletreeTest::testMakeTreeFromPtr01() {
    CryptoKernel::BigNum leftVal = CryptoKernel::BigNum("aBc381023c383Def");
    CryptoKernel::BigNum rightVal = CryptoKernel::BigNum("bAc391045cEE3Dfe");
    const auto leftNode = std::make_shared<CryptoKernel::MerkleNode>(CryptoKernel::MerkleNode(leftVal, rightVal));

    CryptoKernel::BigNum leftVal2 = CryptoKernel::BigNum("cDc381023c383DbE");
    CryptoKernel::BigNum rightVal2 = CryptoKernel::BigNum("cAc391045cEE3DEE");
    const auto rightNode = std::make_shared<CryptoKernel::MerkleNode>(CryptoKernel::MerkleNode(leftVal2, rightVal2));

    CryptoKernel::MerkleNode node = CryptoKernel::MerkleNode(leftNode, rightNode);
    const std::string actualLeft = node.getLeftVal().toString();
    const std::string expectedLeft = leftNode->getMerkleRoot().toString();

    CPPUNIT_ASSERT_EQUAL(expectedLeft, actualLeft);

    const std::string actualRight = node.getRightVal().toString();
    const std::string expectedRight = rightNode->getMerkleRoot().toString();

    CPPUNIT_ASSERT_EQUAL(expectedRight, actualRight);
}

void MerkletreeTest::testMakeTreeFromPtr02() {
    CryptoKernel::BigNum leftVal = CryptoKernel::BigNum("aBc381023c383Def");
    CryptoKernel::BigNum rightVal = CryptoKernel::BigNum("bAc391045cEE3Dfe");
    const auto leftNode = std::make_shared<CryptoKernel::MerkleNode>(CryptoKernel::MerkleNode(leftVal, rightVal));

    CryptoKernel::MerkleNode node = CryptoKernel::MerkleNode(leftNode);
    const std::string actualLeft = node.getLeftVal().toString();
    const std::string expected = leftNode->getMerkleRoot().toString();

    CPPUNIT_ASSERT_EQUAL(expected, actualLeft);

    const std::string actualRight = node.getRightVal().toString();

    CPPUNIT_ASSERT_EQUAL(expected, actualRight);
}

void MerkletreeTest::testMakeTreeFromLeaves() {
    CryptoKernel::BigNum val = CryptoKernel::BigNum("aBc381023c383Def");
    CryptoKernel::BigNum val2 = CryptoKernel::BigNum("bAc391045cEE3Dfe");
    CryptoKernel::BigNum val3 = CryptoKernel::BigNum("cDc381023c383DbE");
    CryptoKernel::BigNum val4 = CryptoKernel::BigNum("cAc391045cEE3DEE");

    const std::set<CryptoKernel::BigNum> nums = {val, val2, val, val4};
    CryptoKernel::MerkleNode node = CryptoKernel::MerkleNode::makeMerkleTree(nums);

    const std::string actualLeft = node.getLeftVal().toString();
    const std::string expectedLeft = "f2be8819c7e4bc3b4e2611c09d26ec93f5094e3cb36930a232654008a0ecaa50";
    
    CPPUNIT_ASSERT_EQUAL(expectedLeft, actualLeft);

    const std::string actualRight = node.getRightVal().toString();
    const std::string expectedRight = "f2be8819c7e4bc3b4e2611c09d26ec93f5094e3cb36930a232654008a0ecaa50";

    CPPUNIT_ASSERT_EQUAL(expectedRight, actualRight);
}



void MerkletreeTest::testMakeProof() {
    const std::string garbage = "33761f1b9133fe8a02b4bfffe94db76efbcfaf8cb1198fe9a41ef4cdfe23cc194ead2f273acada6eb89f851d65191b0a2e6b0a14cb65933ba28ddea67cac5630e60f9039e8fa0180ed9bac97bfc31da5395962ae5312082e9aebf337d12f60c574ff0623b2b6b51b0e75041c6b81fd7651d932ddda1580a98b4e2774b600a40640477f16ca5877c95dda30015c671484d8469ab8306297b5919d915793f83946c1b933b131f4a650f2e42e1c8879d85d48f8b1e8a802ba7d3a49b3e46b14c6690254ca2f495e1c7f6291c9d6c451015b7d1f6b6fc8d5b4fd46d61f2975d154fc51edbd552243a6c14171404131d6261fde5121bb817b36345cd3b1b66d296a577d3623e29bfa0f1e5e2ce42b7a82fb79952c3b190bd67f4404828c14fa5d41d4f1313a8428ed551bee1d9feea01483d0d3c19cfdb7d8652bb6745df459bf06097cf46f3899394bd8cac4002767e216a8c831a28aac3946958c24c2d28e12ed2add7337f8becd60aa3148d16f5c3134af777c441320842c01b313814a80f0b7b8dc57c06c89a3ea5554e070591a8339db913a6175425f2bafb48d91a490de40c681132bf2123bbf53421d346264e7059c4d4bf4c6d91460bd50b22838bd1a408177b2ab9255d222d97730dd995d1e2f7cda505b3c58e58fbc629203ca4142632295838fdb2f47f7c099f5d8e414e0d0ca4ef791be651c175ecc3a88b68850e3b62b4bfffe94db76efbcfc175ecc3a88b68850e3b62b3b62b4bfffe94d";

    for(int j = 1; j < 200; j+=5) {
        std::set<CryptoKernel::BigNum> nums = {};
        for(int i = 0; i < j; i++) {
            nums.insert(CryptoKernel::BigNum(garbage.substr(i,24)));
        }
    
        CryptoKernel::MerkleNode node = CryptoKernel::MerkleNode::makeMerkleTree(nums);
        

        for(int i = 0; i < j; i++) {
            CryptoKernel::BigNum val = CryptoKernel::BigNum(garbage.substr(i,24));
            
            int position = std::distance(nums.begin(), nums.find(val));
            std::shared_ptr<CryptoKernel::MerkleProof> proof = node.makeProof(val);
            CPPUNIT_ASSERT_EQUAL(position, proof->positionInTotalSet);
        }
    }
}

void MerkletreeTest::testVerifyProof() {
    const std::string garbage = "33761f1b9133fe8a02b4bfffe94db76efbcfaf8cb1198fe9a41ef4cdfe23cc194ead2f273acada6eb89f851d65191b0a2e6b0a14cb65933ba28ddea67cac5630e60f9039e8fa0180ed9bac97bfc31da5395962ae5312082e9aebf337d12f60c574ff0623b2b6b51b0e75041c6b81fd7651d932ddda1580a98b4e2774b600a40640477f16ca5877c95dda30015c671484d8469ab8306297b5919d915793f83946c1b933b131f4a650f2e42e1c8879d85d48f8b1e8a802ba7d3a49b3e46b14c6690254ca2f495e1c7f6291c9d6c451015b7d1f6b6fc8d5b4fd46d61f2975d154fc51edbd552243a6c14171404131d6261fde5121bb817b36345cd3b1b66d296a577d3623e29bfa0f1e5e2ce42b7a82fb79952c3b190bd67f4404828c14fa5d41d4f1313a8428ed551bee1d9feea01483d0d3c19cfdb7d8652bb6745df459bf06097cf46f3899394bd8cac4002767e216a8c831a28aac3946958c24c2d28e12ed2add7337f8becd60aa3148d16f5c3134af777c441320842c01b313814a80f0b7b8dc57c06c89a3ea5554e070591a8339db913a6175425f2bafb48d91a490de40c681132bf2123bbf53421d346264e7059c4d4bf4c6d91460bd50b22838bd1a408177b2ab9255d222d97730dd995d1e2f7cda505b3c58e58fbc629203ca4142632295838fdb2f47f7c099f5d8e414e0d0ca4ef791be651c175ecc3a88b68850e3b62b4bfffe94db76efbcfc175ecc3a88b68850e3b62b3b62b4bfffe94d";

    for(int j = 1; j < 200; j+=5) {
        std::set<CryptoKernel::BigNum> nums = {};
        for(int i = 0; i < j; i++) {
            nums.insert(CryptoKernel::BigNum(garbage.substr(i,24)));
        }
    
        CryptoKernel::MerkleNode node = CryptoKernel::MerkleNode::makeMerkleTree(nums);

        for(int i = 0; i < j; i++) {
            CryptoKernel::BigNum val = CryptoKernel::BigNum(garbage.substr(i,24));
            std::shared_ptr<CryptoKernel::MerkleProof> proof = node.makeProof(val);
            std::shared_ptr<CryptoKernel::MerkleNode> proofNode = CryptoKernel::MerkleNode::makeMerkleTreeFromProof(proof);
            CPPUNIT_ASSERT_EQUAL(node.getMerkleRoot().toString(), proofNode->getMerkleRoot().toString());
        }
    }
}

void MerkletreeTest::testProofSerialize() {
    CryptoKernel::BigNum val = CryptoKernel::BigNum("aBc381023c383Def");
    CryptoKernel::BigNum val2 = CryptoKernel::BigNum("bAc391045cEE3Dfe");
    CryptoKernel::BigNum val3 = CryptoKernel::BigNum("cDc381023c383DbE");
    CryptoKernel::BigNum val4 = CryptoKernel::BigNum("cAc391045cEE3DEE");
    const std::set<CryptoKernel::BigNum> nums = {val, val2, val3, val4};

    CryptoKernel::MerkleNode node = CryptoKernel::MerkleNode::makeMerkleTree(nums);
    std::shared_ptr<CryptoKernel::MerkleProof> proof = node.makeProof(val3);

    Json::StreamWriterBuilder builder;
    builder["indentation"] = ""; // If you want whitespace-less output
    const std::string output = Json::writeString(builder, proof->toJson());
    
    const std::string expectedOutput = "{\"leaves\":[\"cdc381023c383dbe\",\"cac391045cee3dee\",\"639d30b6811f703aac5a8296e4878e7ab6eeadf9b05e2821390ed0776bdd96be\",\"381529cb817f5faeee8131a2db231b938c6fbb80b6908bcded60edc87c4ed405\"],\"position\":3}";
    CPPUNIT_ASSERT_EQUAL(output, expectedOutput);
}

void MerkletreeTest::testProofDeserialize() {
    const std::string input = "{\"leaves\":[\"cdc381023c383dbe\",\"cac391045cee3dee\",\"639d30b6811f703aac5a8296e4878e7ab6eeadf9b05e2821390ed0776bdd96be\",\"381529cb817f5faeee8131a2db231b938c6fbb80b6908bcded60edc87c4ed405\"],\"position\":3}";
    const std::string merkleRoot = "ce47e3721c8ce17cf3f81f131f39cd480ae12ca74e8955503a45a0595897b59a";

    Json::Value inputJson;   
    Json::CharReaderBuilder builder;
    Json::CharReader* reader = builder.newCharReader();
    std::string errors;
    bool parsingSuccessful = reader->parse(
        input.c_str(),
        input.c_str() + input.size(),
        &inputJson,
        &errors
    );
    delete reader;

    CPPUNIT_ASSERT(parsingSuccessful);
    
    std::shared_ptr<CryptoKernel::MerkleProof> proof = std::make_shared<CryptoKernel::MerkleProof>(inputJson);
            
    CPPUNIT_ASSERT_EQUAL(3, proof->positionInTotalSet);
    CPPUNIT_ASSERT_EQUAL(CryptoKernel::BigNum("cdc381023c383dbe").toString(), proof->leaves.at(0).toString());

    std::shared_ptr<CryptoKernel::MerkleNode> proofNode = CryptoKernel::MerkleNode::makeMerkleTreeFromProof(proof);

    CPPUNIT_ASSERT_EQUAL(merkleRoot, proofNode->getMerkleRoot().toString());
}

void MerkletreeTest::testProofDeserializeInvalid() {
    const std::string input = "{\"garbage\":1}";
     Json::Value inputJson;   
    Json::CharReaderBuilder builder;
    Json::CharReader* reader = builder.newCharReader();
    std::string errors;
    bool parsingSuccessful = reader->parse(
        input.c_str(),
        input.c_str() + input.size(),
        &inputJson,
        &errors
    );
    delete reader;

    CPPUNIT_ASSERT_THROW(std::make_shared<CryptoKernel::MerkleProof>(inputJson), CryptoKernel::Blockchain::InvalidElementException);
}