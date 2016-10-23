#!/bin/sh

exe() { echo "$@" ; "$@" ; }

CXX=g++
CXXFLAGS="-Wall -std=c++14 -O2 -fPIC"

exe ${CXX} ${CXXFLAGS} -c tests/CryptoKernelTestRunner.cpp -o tests/CryptoKernelTestRunner.o
exe ${CXX} ${CXXFLAGS} -c tests/CryptoTests.cpp -o tests/CryptoTests.o
exe ${CXX} -o test tests/CryptoKernelTestRunner.o tests/CryptoTests.o -Isrc -L. -lCryptoKernel -lcrypto -pthread -ldl -lcppunit
./test
