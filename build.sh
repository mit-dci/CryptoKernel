#!/bin/sh

exe() { echo "$@" ; "$@" ; }

CXX=g++
CXXFLAGS="-Wall -std=c++14 -O2 -fPIC"

exe ${CXX} ${CXXFLAGS} -c src/blockchain.cpp -o src/blockchain.o
exe ${CXX} ${CXXFLAGS} -c src/base64.cpp -o src/base64.o
exe ${CXX} ${CXXFLAGS} -c src/crypto.cpp -o src/crypto.o
exe ${CXX} ${CXXFLAGS} -c src/log.cpp -o src/log.o
exe ${CXX} ${CXXFLAGS} -c src/math.cpp -o src/math.o
exe ${CXX} ${CXXFLAGS} -c src/network.cpp -o src/network.o
exe ${CXX} ${CXXFLAGS} -c src/storage.cpp -o src/storage.o
exe ar -r -s libCryptoKernel.a src/base64.o src/blockchain.o src/crypto.o src/log.o src/math.o src/network.o src/storage.o
