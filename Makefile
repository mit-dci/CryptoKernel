UNAME := $(shell uname -o)

ifeq ($(UNAME), GNU/Linux)
LUA_INCDIR ?= /usr/local/include/lua5.3
LUA_LIBDIR ?= /usr/local/lib
LIBFLAGS   ?= -shared
BINFLAGS   ?= -I. -L. -lck -llua5.3 -lcrypto -ldl -pthread -lleveldb -lmicrohttpd -ljsonrpccpp-common -ljsonrpccpp-server -lcurl -ljsonrpccpp-client -ljsoncpp -lsfml-system -lsfml-network
PLATFORMCXXFLAGS ?= -fPIC
CKLIB ?= libck.so
CKBIN ?= ckd
TESTBIN ?= test-ck
CC = g++
endif
ifeq ($(UNAME), Msys)
LUA_INCDIR ?= /usr/local/include
LUA_LIBDIR ?= /usr/local/lib
LIBFLAGS   ?= -shared
BINFLAGS   ?= -I. -L. -lck -llua -ljsoncpp -lcrypto -pthread -lleveldb -ljsonrpccpp-server -ljsonrpccpp-client -ljsonrpccpp-common -lmicrohttpd -lcurl -lsfml-system -lsfml-network -lws2_32
KERNELLIBS ?= -llua -ljsoncpp -lcrypto -pthread -lleveldb -lsfml-system -lsfml-network -lws2_32
CKLIB ?= libck.dll
CKBIN ?= ckd.exe
TESTBIN ?= test-ck.exe
CC = g++
endif

KERNELCXXFLAGS ?= -g -Wall -std=c++14 -O2 -Wl,-E -Isrc/kernel

KERNELSRC = src/kernel/blockchain.cpp src/kernel/math.cpp src/kernel/storage.cpp src/kernel/network.cpp src/kernel/networkpeer.cpp src/kernel/base64.cpp src/kernel/crypto.cpp src/kernel/log.cpp src/kernel/contract.cpp src/kernel/consensus/AVRR.cpp src/kernel/consensus/PoW.cpp
KERNELOBJS = $(KERNELSRC:.cpp=.o)

CLIENTSRC = src/client/main.cpp src/client/rpcserver.cpp src/client/wallet.cpp
CLIENTOBJS = $(CLIENTSRC:.cpp=.o)

TESTSRC = tests/CryptoKernelTestRunner.cpp tests/CryptoTests.cpp tests/MathTests.cpp tests/StorageTests.cpp tests/LogTests.cpp tests/ContractTests.cpp
TESTOBJS = $(TESTSRC:.cpp=.o)

CXXFLAGS = $(KERNELCXXFLAGS) $(PLATFORMCXXFLAGS) -I$(LUA_INCDIR)
KERNELLDFLAGS = $(LIBFLAGS) -L$(LUA_LIBDIR)
CLIENTLDFLAGS = $(BINFLAGS) -L$(LUA_LIBDIR)
TESTLDFLAGS = $(CLIENTLDFLAGS) -lcppunit -Wl,-rpath -Wl,./

all: daemon

lib: $(KERNELSRC) $(CKLIB)

daemon: $(CKLIB) $(CLIENTSRC) $(CKBIN)

doc: 
	doxygen .

test: $(CKLIB) $(TESTSRC) $(TESTBIN)
	./$(TESTBIN)

$(TESTBIN): $(TESTOBJS)
	$(CC) $(TESTOBJS) -o $@ $(TESTLDFLAGS)

clean:
	$(RM) $(CLIENTOBJS) $(KERNELOBJS) $(TESTOBJS) $(CKLIB) $(CKBIN) $(TESTBIN) ./doxygen

$(CKBIN): $(CLIENTOBJS)
	$(CC) $(CLIENTOBJS) -o $@ $(CLIENTLDFLAGS)
	
$(CKLIB): $(KERNELOBJS)
	$(CC) $(KERNELLDFLAGS) $(KERNELOBJS) -o $@ $(KERNELLIBS)

.o: 
	$(CC) $(CXXFLAGS) $< -o $@

