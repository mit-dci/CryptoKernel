#!/bin/sh

exe() { echo "$@" ; "$@" ; }

CXX=g++
CXXFLAGS="-Wall -std=c++14 -O2 -fPIC"

exe ${CXX} ${CXXFLAGS} -c blockchain.cpp -o blockchain.o
exe ${CXX} ${CXXFLAGS} -c base64.cpp -o base64.o
exe ${CXX} ${CXXFLAGS} -c crypto.cpp -o crypto.o
exe ${CXX} ${CXXFLAGS} -c log.cpp -o log.o
exe ${CXX} ${CXXFLAGS} -c math.cpp -o math.o
exe ${CXX} ${CXXFLAGS} -c network.cpp -o network.o
exe ${CXX} ${CXXFLAGS} -c storage.cpp -o storage.o
exe ar -r -s libCryptoKernel.a base64.o blockchain.o crypto.o log.o math.o network.o storage.o