FROM ubuntu:18.04

ENV CC=x86_64-w64-mingw32-gcc-posix
ENV CXX=x86_64-w64-mingw32-g++-posix
ENV AR=x86_64-w64-mingw32-ar

RUN apt update && apt dist-upgrade -y && \
    apt install -y make git cmake g++-mingw-w64-x86-64 \
    binutils-mingw-w64-x86-64 wget bison flex libtool automake

RUN wget https://github.com/open-source-parsers/jsoncpp/archive/1.8.4.tar.gz \
    && tar -xzvf 1.8.4.tar.gz
WORKDIR jsoncpp-1.8.4
RUN cmake . -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=$CC \
    -DCMAKE_CXX_COMPILER=$CXX -DJSONCPP_WITH_TESTS=NO -DBUILD_STATIC_LIBS=YES \
    -DBUILD_SHARED_LIBS=NO -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local/mingw
RUN make && make install

WORKDIR ../

RUN wget https://www.openssl.org/source/openssl-1.1.0h.tar.gz && \
    tar -xvzf openssl-1.1.0h.tar.gz
WORKDIR openssl-1.1.0h
RUN ./Configure mingw64 --prefix=/usr/local/mingw no-shared
RUN make && make install

WORKDIR ../

RUN wget https://curl.haxx.se/download/curl-7.59.0.tar.gz && \
    tar xzvf curl-7.59.0.tar.gz
WORKDIR curl-7.59.0
RUN ./configure --host=x86_64-w64-mingw32 --prefix=/usr/local/mingw \
    --disable-shared --enable-static --disable-ldap
RUN make && make install

WORKDIR ../

RUN wget http://ftp.gnu.org/gnu/libmicrohttpd/libmicrohttpd-0.9.59.tar.gz && \
    tar -xvzf libmicrohttpd-0.9.59.tar.gz
WORKDIR libmicrohttpd-0.9.59
RUN ./configure --host=x86_64-w64-mingw32 --prefix=/usr/local/mingw \
    --disable-shared
RUN make && make install

WORKDIR ../

RUN wget https://github.com/cinemast/libjson-rpc-cpp/archive/v1.1.0.tar.gz && \
    tar -xvzf v1.1.0.tar.gz
WORKDIR libjson-rpc-cpp-1.1.0 
RUN cmake -E env CXXFLAGS="-DCURL_STATICLIB" cmake . -DBUILD_SHARED_LIBS=NO \
    -DBUILD_STATIC_LIBS=YES -DCOMPILE_TESTS=NO \
    -DCOMPILE_STUBGEN=NO -DCOMPILE_EXAMPLES=NO -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local/mingw -DREDIS_CLIENT=NO -DREDIS_SERVER=NO \
    -DWITH_COVERAGE=NO
RUN mkdir -p win32-deps/include && make && make install

WORKDIR ../

RUN git clone https://github.com/rweather/noise-c
WORKDIR noise-c
ADD noise-c-windows.patch .
RUN git apply noise-c-windows.patch
RUN ./autogen.sh && ./configure --host=x86_64-linux --prefix=/usr/local/mingw
RUN make && make install

WORKDIR ../

RUN git clone https://github.com/bitcoin-core/leveldb
WORKDIR leveldb

ADD 0001-Fixes-for-windows-cross-compile.patch .
RUN patch < 0001-Fixes-for-windows-cross-compile.patch
RUN TARGET_OS=OS_WINDOWS_CROSSCOMPILE make
RUN cp -r out-static/*.a /usr/local/mingw/lib && \
    cp -r include/* /usr/local/mingw/include

WORKDIR ../

RUN git clone https://github.com/SFML/SFML.git
WORKDIR SFML
RUN cmake . -DBUILD_SHARED_LIBS=NO -DSFML_BUILD_DOC=NO -DSFML_BUILD_AUDIO=NO \
    -DSFML_BUILD_GRAPHICS=NO -DSFML_BUILD_WINDOW=NO -DSFML_BUILD_EXAMPLES=NO \
    -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=$CC \
    -DCMAKE_CXX_COMPILER=$CXX -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local/mingw
RUN make && make install
RUN cp /usr/local/mingw/lib/libsfml-network-s.a /usr/local/mingw/lib/libsfml-network.a && cp /usr/local/mingw/lib/libsfml-system-s.a /usr/local/mingw/lib/libsfml-system.a

WORKDIR ../

RUN git clone https://github.com/lhorgan/luack
WORKDIR luack
RUN git checkout 43e9e17984e4e992ac2dd0510ac15ebd22f38fdc
RUN make mingw CC=$CC && \
    cp src/lua53.dll /usr/local/mingw/lib/liblua.dll && \
    cp src/*.h /usr/local/mingw/include/

WORKDIR ../

RUN git clone https://github.com/metalicjames/selene.git
RUN cp -r selene/include/* /usr/local/mingw/include

RUN git clone https://github.com/metalicjames/lua-lz4.git
WORKDIR lua-lz4
RUN CC=x86_64-w64-mingw32-gcc make UNAME=x86_64-w64-mingw32 

WORKDIR ../

RUN wget https://github.com/premake/premake-core/releases/download/v5.0.0-alpha12/premake-5.0.0-alpha12-linux.tar.gz && \
    tar zxvf premake-5.0.0-alpha12-linux.tar.gz && \
    cp premake5 /usr/bin

COPY ./cryptokernel /cryptokernel

RUN cp lua-lz4/lz4.dll cryptokernel
RUN cp /usr/local/mingw/lib/liblua.dll cryptokernel/lua53.dll

RUN git clone https://github.com/metalicjames/cschnorr.git
WORKDIR cschnorr
RUN premake5 gmake2 --os=windows --include-dir=/usr/local/mingw/include && make config=release_static cschnorr && \
    mkdir /usr/local/mingw/include/cschnorr/ && \
    cp src/*.h /usr/local/mingw/include/cschnorr/ && \
    cp bin/Static/Release/cschnorr.lib /usr/local/mingw/lib
WORKDIR ../

WORKDIR cryptokernel
RUN premake5 gmake2 --os=windows --include-dir=/usr/local/mingw/include --lib-dir=/usr/local/mingw/lib
RUN make config=release_static ckd
