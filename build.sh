#!/bin/sh

exe() { echo "$@" ; "$@" ; }

CXX=g++
CXXFLAGS="-g -Wall -std=c++14 -O2 -fPIC -Wl,-E -I/usr/include/lua5.3"
SRC_DIR="src/kernel"
OBJ_DIR="obj"

exe mkdir ${OBJ_DIR}

exe ${CXX} ${CXXFLAGS} -c ${SRC_DIR}/blockchain.cpp -o ${OBJ_DIR}/blockchain.o
exe ${CXX} ${CXXFLAGS} -c ${SRC_DIR}/base64.cpp -o ${OBJ_DIR}/base64.o
exe ${CXX} ${CXXFLAGS} -c ${SRC_DIR}/crypto.cpp -o ${OBJ_DIR}/crypto.o
exe ${CXX} ${CXXFLAGS} -c ${SRC_DIR}/log.cpp -o ${OBJ_DIR}/log.o
exe ${CXX} ${CXXFLAGS} -c ${SRC_DIR}/math.cpp -o ${OBJ_DIR}/math.o
exe ${CXX} ${CXXFLAGS} -c ${SRC_DIR}/networkserver.cpp -o ${OBJ_DIR}/networkserver.o
exe ${CXX} ${CXXFLAGS} -c ${SRC_DIR}/networkclient.cpp -o ${OBJ_DIR}/networkclient.o
exe ${CXX} ${CXXFLAGS} -c ${SRC_DIR}/network.cpp -o ${OBJ_DIR}/network.o
exe ${CXX} ${CXXFLAGS} -c ${SRC_DIR}/storage.cpp -o ${OBJ_DIR}/storage.o
exe ${CXX} ${CXXFLAGS} -c ${SRC_DIR}/contract.cpp -o ${OBJ_DIR}/contract.o
exe ar -r -s libCryptoKernel.a ${OBJ_DIR}/base64.o ${OBJ_DIR}/blockchain.o ${OBJ_DIR}/crypto.o ${OBJ_DIR}/log.o ${OBJ_DIR}/math.o ${OBJ_DIR}/storage.o ${OBJ_DIR}/contract.o ${OBJ_DIR}/networkserver.o ${OBJ_DIR}/networkclient.o ${OBJ_DIR}/network.o
