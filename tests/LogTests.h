#ifndef LOGTEST_H
#define LOGTEST_H

#include <cppunit/extensions/HelperMacros.h>

#include "log.h"

class LogTest : public CPPUNIT_NS::TestFixture {
    CPPUNIT_TEST_SUITE(LogTest);

    CPPUNIT_TEST(testLogging);

    CPPUNIT_TEST_SUITE_END();

public:
    LogTest();
    virtual ~LogTest();
    void setUp();
    void tearDown();

private:
    void testLogging();
};

#endif
