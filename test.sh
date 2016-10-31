#!/bin/sh

exe() { echo "$@" ; "$@" ; }

CXX=g++
CXXFLAGS="-Wall -std=c++14 -O2 -fPIC"
INCLUDES="-Isrc"

exe ${CXX} ${CXXFLAGS} ${INCLUDES} -c tests/CryptoKernelTestRunner.cpp -o tests/CryptoKernelTestRunner.o
exe ${CXX} ${CXXFLAGS} ${INCLUDES} -c tests/CryptoTests.cpp -o tests/CryptoTests.o
exe ${CXX} ${CXXFLAGS} ${INCLUDES} -c tests/MathTests.cpp -o tests/MathTests.o
exe ${CXX} -o test tests/CryptoKernelTestRunner.o tests/CryptoTests.o tests/MathTests.o -L. -lCryptoKernel -lcrypto -pthread -ldl -lcppunit
./test
