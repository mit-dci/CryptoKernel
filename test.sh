#!/bin/sh

exe() { echo "$@" ; "$@" ; }

CXX=g++
CXXFLAGS="-Wall -std=c++14 -O2 -fPIC -Wl,-E -g -I/usr/include/lua5.3"
INCLUDES="-Isrc/kernel"
LIBS="-lCryptoKernel -lcrypto -lleveldb -ljsoncpp -pthread -lcppunit -llua5.3 -ldl"

exe ${CXX} ${CXXFLAGS} ${INCLUDES} -c tests/CryptoKernelTestRunner.cpp -o tests/CryptoKernelTestRunner.o
exe ${CXX} ${CXXFLAGS} ${INCLUDES} -c tests/CryptoTests.cpp -o tests/CryptoTests.o
exe ${CXX} ${CXXFLAGS} ${INCLUDES} -c tests/MathTests.cpp -o tests/MathTests.o
exe ${CXX} ${CXXFLAGS} ${INCLUDES} -c tests/StorageTests.cpp -o tests/StorageTests.o
exe ${CXX} ${CXXFLAGS} ${INCLUDES} -c tests/LogTests.cpp -o tests/LogTests.o
exe ${CXX} ${CXXFLAGS} ${INCLUDES} -c tests/ContractTests.cpp -o tests/ContractTests.o
exe ${CXX} -o test tests/CryptoKernelTestRunner.o tests/StorageTests.o tests/CryptoTests.o tests/MathTests.o tests/LogTests.o tests/ContractTests.o -L. ${LIBS} 
./test
