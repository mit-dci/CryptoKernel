#!/bin/sh

CXX=g++
CXXFLAGS=-Wall -std=c++14 -std=c++14 -O2 -fPIC

${CXX} ${CXXFLAGS} -c blockchain.cpp -o blockchain.o
${CXX} ${CXXFLAGS} -c base64.cpp -o base64.o
${CXX} ${CXXFLAGS} -c crypto.cpp -o crypto.o
${CXX} ${CXXFLAGS} -c log.cpp -o log.o
${CXX} ${CXXFLAGS} -c math.cpp -o math.o
${CXX} ${CXXFLAGS} -c network.cpp -o network.o
${CXX} ${CXXFLAGS} -c storage.cpp -o storage.o
ar -r -s libCryptoKernel.a base64.o blockchain.o crypto.o log.o math.o network.o storage.o