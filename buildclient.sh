#!/bin/sh

exe() { echo "$@" ; "$@" ; }

CXX=g++
CXXFLAGS="-g -Wall -std=c++14 -O2 -fPIC -Wl,-E -Isrc/kernel -I/usr/include/lua5.3"
SRC_DIR="src/client"
OBJ_DIR="obj"
LIBS="-lCryptoKernel -llua5.3 -ljsoncpp -lcrypto -ldl -pthread -lleveldb -lmicrohttpd -ljsonrpccpp-common -ljsonrpccpp-server -lcurl -ljsonrpccpp-client"

exe mkdir ${OBJ_DIR}

exe ${CXX} ${CXXFLAGS} -c ${SRC_DIR}/main.cpp -o ${OBJ_DIR}/main.o
exe ${CXX} ${CXXFLAGS} -c ${SRC_DIR}/rpcserver.cpp -o ${OBJ_DIR}/rpcserver.o
exe ${CXX} ${CXXFLAGS} -c ${SRC_DIR}/wallet.cpp -o ${OBJ_DIR}/wallet.o
exe ${CXX} -o CryptoCurrency ${OBJ_DIR}/main.o ${OBJ_DIR}/rpcserver.o ${OBJ_DIR}/wallet.o -L. ${LIBS}
