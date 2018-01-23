UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
LUA_INCDIR ?= /usr/include/lua5.3
LUA_LIBDIR ?= /usr/lib
LIBFLAGS   ?= -shared
BINFLAGS   ?= -I. -L. -L/usr/local/lib -Wl,-rpath -Wl,./ -lck -llua5.3 -lcrypto -ldl -pthread -lleveldb -lmicrohttpd -ljsonrpccpp-common -ljsonrpccpp-server -lcurl -ljsonrpccpp-client -ljsoncpp -lsfml-system -lsfml-network
PLATFORMCXXFLAGS ?= -fPIC
CKLIB ?= libck.so
CKBIN ?= ckd
TESTBIN ?= test-ck
CC = g++
C = gcc
endif
ifeq ($(UNAME), MINGW32_NT-6.2)
LUA_INCDIR ?= /usr/local/include
LUA_LIBDIR ?= /usr/local/lib
LIBFLAGS   ?= -shared
BINFLAGS   ?= -I. -L. -lck -llua -ljsoncpp -lcrypto -pthread -lleveldb -ljsonrpccpp-server -ljsonrpccpp-client -ljsonrpccpp-common -lmicrohttpd -lcurl -lsfml-system -lsfml-network -lws2_32
KERNELLIBS ?= -llua -ljsoncpp -lcrypto -pthread -lleveldb -lsfml-system -lsfml-network -lws2_32
CKLIB ?= libck.dll
CKBIN ?= ckd.exe
TESTBIN ?= test-ck.exe
CC = g++
C = gcc
endif
ifeq ($(UNAME), Darwin)
LUA_INCDIR ?= /opt/local/include
LUA_LIBDIR ?= /opt/local/lib
LIBFLAGS   ?= -shared -stdlib=libc++ -std=c++14
BINFLAGS   ?= -I. -L. -lck -llua -ljsoncpp -lcrypto -lleveldb -ljsonrpccpp-server -ljsonrpccpp-client -ljsonrpccpp-common -lmicrohttpd -lcurl -lsfml-system -lsfml-network
KERNELLIBS ?= -llua -ljsoncpp -lcrypto.1.1 -lleveldb -lsfml-system -lsfml-network
KERNELCXXFLAGS += -stdlib=libc++
CKLIB ?= libck.dylib
CKBIN ?= ckd
TESTBIN ?= test-ck
CC = clang++
C = clang
endif
ifeq ($(UNAME), Darwin-Cross)
LUA_INCDIR ?= /home/james/osxcross/target/macports/pkgs/opt/local/include
LUA_LIBDIR ?= /home/james/osxcross/target/macports/pkgs/opt/local/lib
LIBFLAGS   ?= -shared -stdlib=libc++ -std=c++14
BINFLAGS   ?= -I. -L. -lck -llua -ljsoncpp -lcrypto.1.1 -lleveldb -ljsonrpccpp-server -ljsonrpccpp-client -ljsonrpccpp-common -lmicrohttpd -lcurl -lsfml-system -lsfml-network
KERNELLIBS ?= -llua -ljsoncpp -lcrypto.1.1 -lleveldb -lsfml-system -lsfml-network
KERNELCXXFLAGS += -stdlib=libc++
CKLIB ?= libck.dylib
CKBIN ?= ckd
TESTBIN ?= test-ck
CC = o64-clang++
C = o64-clang
endif

KERNELCXXFLAGS += -g -Wall -std=c++14 -O2 -Wl,-E -Isrc/kernel

KERNELSRC = src/kernel/blockchain.cpp src/kernel/blockchaintypes.cpp src/kernel/math.cpp src/kernel/storage.cpp src/kernel/network.cpp src/kernel/networkpeer.cpp src/kernel/base64.cpp src/kernel/crypto.cpp src/kernel/log.cpp src/kernel/contract.cpp src/kernel/consensus/AVRR.cpp src/kernel/consensus/PoW.cpp src/kernel/merkletree.cpp src/kernel/consensus/raft.cpp
KERNELOBJS = $(KERNELSRC:.cpp=.cpp.o)

LYRASRC = src/kernel/consensus/Lyra2REv2/Lyra2RE.c src/kernel/consensus/Lyra2REv2/Lyra2.c src/kernel/consensus/Lyra2REv2/Sponge.c src/kernel/consensus/Lyra2REv2/sha3/blake.c src/kernel/consensus/Lyra2REv2/sha3/cubehash.c src/kernel/consensus/Lyra2REv2/sha3/keccak.c src/kernel/consensus/Lyra2REv2/sha3/skein.c src/kernel/consensus/Lyra2REv2/sha3/bmw.c
LYRAOBJS = $(LYRASRC:.c=.c.o)

CLIENTSRC = src/client/main.cpp src/client/rpcserver.cpp src/client/wallet.cpp src/client/httpserver.cpp
CLIENTOBJS = $(CLIENTSRC:.cpp=.cpp.o)

TESTSRC = tests/CryptoKernelTestRunner.cpp tests/CryptoTests.cpp tests/MathTests.cpp tests/StorageTests.cpp tests/LogTests.cpp tests/ContractTests.cpp
TESTOBJS = $(TESTSRC:.cpp=.cpp.o)

CXXFLAGS = $(KERNELCXXFLAGS) $(PLATFORMCXXFLAGS) -I$(LUA_INCDIR)
KERNELLDFLAGS = $(LIBFLAGS) -L$(LUA_LIBDIR)
CLIENTLDFLAGS = -L$(LUA_LIBDIR) $(BINFLAGS)
TESTLDFLAGS = $(CLIENTLDFLAGS) -lcppunit

all: daemon

lib: $(KERNELSRC) $(LYRASRC) $(CKLIB)

daemon: $(CKLIB) $(CLIENTSRC) $(CKBIN)

doc:
	doxygen ./doxyfile

test: $(CKLIB) $(TESTSRC) $(TESTBIN)
	./$(TESTBIN)

$(TESTBIN): $(TESTOBJS)
	$(CC) $(TESTOBJS) -o $@ $(TESTLDFLAGS)

clean:
	$(RM) -r  $(CLIENTOBJS) $(KERNELOBJS) $(LYRAOBJS) $(TESTOBJS) $(CKLIB) $(CKBIN) docs

$(CKBIN): $(CLIENTOBJS) $(CKLIB)
	$(CC) $(CLIENTOBJS) -o $@ $(CLIENTLDFLAGS)

$(CKLIB): $(KERNELOBJS) $(LYRAOBJS)
	$(CC) $(KERNELLDFLAGS) $(KERNELOBJS) $(LYRAOBJS) -o $@ $(KERNELLIBS)

%.c.o: %.c
	$(C) $(PLATFORMCXXFLAGS) -O3 -c $< -o $@

%.cpp.o: %.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@

