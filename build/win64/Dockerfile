FROM alpine-mingw-w64

RUN apk add --no-cache make automake autoconf libtool git cmake

RUN wget https://github.com/open-source-parsers/jsoncpp/archive/1.8.4.tar.gz && tar -xzvf 1.8.4.tar.gz
WORKDIR jsoncpp-1.8.4
RUN cmake . -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ -DJSONCPP_WITH_TESTS=NO -DBUILD_STATIC_LIBS=YES -DBUILD_SHARED_LIBS=NO -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local/mingw
RUN make && make install

WORKDIR ../

RUN wget https://www.openssl.org/source/openssl-1.1.0h.tar.gz && tar -xvzf openssl-1.1.0h.tar.gz
WORKDIR openssl-1.1.0h
RUN CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar ./Configure mingw64 --prefix=/usr/local/mingw no-shared
RUN make && make install

WORKDIR ../

RUN wget https://curl.haxx.se/download/curl-7.59.0.tar.gz && tar xzvf curl-7.59.0.tar.gz
WORKDIR curl-7.59.0
RUN CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar ./configure --host=x86_64-w64-mingw32 --prefix=/usr/local/mingw
RUN make && make install

WORKDIR ../

RUN wget http://ftp.gnu.org/gnu/libmicrohttpd/libmicrohttpd-0.9.59.tar.gz && tar -xvzf libmicrohttpd-0.9.59.tar.gz
WORKDIR libmicrohttpd-0.9.59
RUN CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar ./configure --host=x86_64-w64-mingw32 --prefix=/usr/local/mingw --disable-shared
RUN make && make install

WORKDIR ../

RUN wget https://github.com/cinemast/libjson-rpc-cpp/archive/v1.1.0.tar.gz && tar -xvzf v1.1.0.tar.gz
WORKDIR libjson-rpc-cpp-1.1.0
RUN cmake . -DBUILD_SHARED_LIBS=NO -DBUILD_STATIC_LIBS=YES -DCOMPILE_TESTS=NO -DCOMPILE_STUBGEN=NO -DCOMPILE_EXAMPLES=NO -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local/mingw -DREDIS_CLIENT=NO -DREDIS_SERVER=NO -DWITH_COVERAGE=NO
RUN mkdir -p win32-deps/include && make && make install

WORKDIR ../

RUN git clone https://github.com/bitcoin-core/leveldb
WORKDIR leveldb

ADD 0001-Fixes-for-windows-cross-compile.patch .
RUN patch < 0001-Fixes-for-windows-cross-compile.patch
RUN TARGET_OS=OS_WINDOWS_CROSSCOMPILE CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++ AR=x86_64-w64-mingw32-ar make
RUN cp -r out-static/*.a /usr/local/mingw/lib && cp -r include/* /usr/local/mingw/include

WORKDIR ../

RUN git clone https://github.com/SFML/SFML.git
WORKDIR SFML
RUN cmake . -DBUILD_SHARED_LIBS=YES -DSFML_BUILD_DOC=NO -DSFML_BUILD_AUDIO=NO -DSFML_BUILD_GRAPHICS=NO -DSFML_BUILD_WINDOW=NO -DSFML_BUILD_EXAMPLES=NO -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local/mingw
RUN make && make install
#RUN cp /usr/local/mingw/lib/libsfml-network-s.a /usr/local/mingw/lib/libsfml-network.a && cp /usr/local/mingw/lib/libsfml-system-s.a /usr/local/mingw/lib/libsfml-system.a

WORKDIR ../

RUN wget https://www.lua.org/ftp/lua-5.3.4.tar.gz && tar xzvf lua-5.3.4.tar.gz
WORKDIR lua-5.3.4
RUN make generic CC="x86_64-w64-mingw32-gcc -std=gnu99" AR="x86_64-w64-mingw32-ar rcu" && make local && cp -r install/* /usr/local/mingw

WORKDIR ../

RUN git clone https://github.com/metalicjames/selene.git
RUN cp -r selene/include/* /usr/local/mingw/include

RUN git clone https://github.com/metalicjames/lua-lz4.git
WORKDIR lua-lz4
RUN CC=x86_64-w64-mingw32-gcc make UNAME=x86_64-w64-mingw32 

WORKDIR ../

RUN git clone https://github.com/mit-dci/cryptokernel

RUN cp lua-lz4/lz4.dll cryptokernel

WORKDIR cryptokernel
ADD Makefile .
RUN make LUA_LIBDIR=/usr/local/mingw/lib LUA_INCDIR=/usr/local/mingw/include UNAME=MINGW32_NT-6.2 CC=x86_64-w64-mingw32-g++ C=x86_64-w64-mingw32-gcc
