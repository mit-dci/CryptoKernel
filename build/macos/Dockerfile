FROM ubuntu:18.04

RUN apt update && apt dist-upgrade -y && apt install -y clang git make wget cmake

RUN git clone https://github.com/tpoechtrager/osxcross.git
WORKDIR osxcross

COPY MacOSX10.11.sdk.tar.xz ./tarballs/
RUN SDK_VERSION=10.11 UNATTENDED=1 ./build.sh

ENV PATH="/osxcross/target/bin:${PATH}"
ENV MACOSX_DEPLOYMENT_TARGET=10.9

RUN UNATTENDED=1 osxcross-macports install lua jsoncpp-devel leveldb curl libmicrohttpd sfml

WORKDIR ../

ENV CC=o64-clang
ENV CXX=o64-clang++
ENV AR=x86_64-apple-darwin15-ar
ENV LD=x86_64-apple-darwin15-ld
ENV RANLIB=x86_64-apple-darwin15-ranlib

RUN wget https://www.openssl.org/source/openssl-1.1.0h.tar.gz && tar -xvzf openssl-1.1.0h.tar.gz
WORKDIR openssl-1.1.0h
RUN ./Configure darwin64-x86_64-cc --prefix=/osxcross/target/macports/pkgs/opt/local
RUN make && make install

WORKDIR ../

RUN wget https://github.com/cinemast/libjson-rpc-cpp/archive/v1.1.0.tar.gz && tar -xvzf v1.1.0.tar.gz
WORKDIR libjson-rpc-cpp-1.1.0
RUN cmake . -DBUILD_SHARED_LIBS=NO -DBUILD_STATIC_LIBS=YES -DCOMPILE_TESTS=NO -DCOMPILE_STUBGEN=NO -DCOMPILE_EXAMPLES=NO -DCMAKE_SYSTEM_NAME=Darwin -DCMAKE_C_COMPILER=$CC -DCMAKE_AR=/osxcross/target/bin/$AR -DCMAKE_CXX_COMPILER=$CXX -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/osxcross/target/macports/pkgs/opt/local -DCMAKE_INCLUDE_PATH=/osxcross/target/macports/pkgs/opt/local/include -DREDIS_CLIENT=NO -DREDIS_SERVER=NO -DWITH_COVERAGE=NO 
RUN OSXCROSS_MP_INC=1 make && make install

WORKDIR ../

RUN git clone https://github.com/metalicjames/selene.git
RUN cp -r selene/include/* /osxcross/target/macports/pkgs/opt/local/include

RUN git clone https://github.com/metalicjames/lua-lz4.git
WORKDIR lua-lz4
RUN LUA_INCDIR=/osxcross/target/macports/pkgs/opt/local/include LUA_LIBDIR=/osxcross/target/macports/pkgs/opt/local/lib make UNAME=Darwin 

WORKDIR ../

RUN git clone https://github.com/mit-dci/cryptokernel

RUN cp lua-lz4/lz4.so cryptokernel

WORKDIR cryptokernel
RUN make LUA_INCDIR=/osxcross/target/macports/pkgs/opt/local/include LUA_LIBDIR=/osxcross/target/macports/pkgs/opt/local/lib UNAME=Darwin-Cross CC=$CXX C=$CC