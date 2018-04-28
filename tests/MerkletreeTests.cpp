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
