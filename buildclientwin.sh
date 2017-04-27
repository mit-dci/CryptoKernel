#!/bin/sh

exe() { echo "$@" ; "$@" ; }

CXX=g++
CXXFLAGS="-g -Wall -std=c++14 -O2 -Wl,-E -Isrc/kernel -I/usr/include/lua5.3"
SRC_DIR="src/client"
OBJ_DIR="obj"
LIBS="-L/usr/local/lib -lCryptoKernel -llua -ljsoncpp -lcrypto -pthread -lleveldb -ljsonrpccpp-server -ljsonrpccpp-client -ljsonrpccpp-common -lmicrohttpd -lcurl -lsfml-system -lsfml-network -lws2_32"

exe mkdir ${OBJ_DIR}

exe ${CXX} ${CXXFLAGS} -c ${SRC_DIR}/main.cpp -o ${OBJ_DIR}/main.o
exe ${CXX} ${CXXFLAGS} -c ${SRC_DIR}/rpcserver.cpp -o ${OBJ_DIR}/rpcserver.o
exe ${CXX} ${CXXFLAGS} -c ${SRC_DIR}/wallet.cpp -o ${OBJ_DIR}/wallet.o
exe ${CXX} -o CryptoCurrency ${OBJ_DIR}/main.o ${OBJ_DIR}/rpcserver.o ${OBJ_DIR}/wallet.o -L. ${LIBS}
