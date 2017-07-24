#include "LogTests.h"

CPPUNIT_TEST_SUITE_REGISTRATION(LogTest);

LogTest::LogTest() {
    std::remove("testLog.log");
}

LogTest::~LogTest() {
}

void LogTest::setUp() {
}

void LogTest::tearDown() {
}

void LogTest::testLogging() {
    CryptoKernel::Log log("testLog.log");

    CPPUNIT_ASSERT(log.getStatus());

    CPPUNIT_ASSERT(log.printf(LOG_LEVEL_INFO, "Test message"));
}
