FROM ubuntu:18.04

RUN apt update && apt dist-upgrade -y && apt install -y clang git make wget cmake libtool automake bison flex

RUN git clone https://github.com/tpoechtrager/osxcross.git
WORKDIR osxcross

COPY MacOSX10.11.sdk.tar.xz ./tarballs/
RUN SDK_VERSION=10.11 UNATTENDED=1 ./build.sh

ENV PATH="/osxcross/target/bin:${PATH}"
ENV MACOSX_DEPLOYMENT_TARGET=10.9

RUN UNATTENDED=1 osxcross-macports install jsoncpp-devel leveldb curl libmicrohttpd sfml readline

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

RUN git clone https://github.com/rweather/noise-c
WORKDIR noise-c
ADD noise-c-macos.patch .
RUN git apply noise-c-macos.patch
RUN ./autogen.sh && ./configure --prefix=/osxcross/target/macports/pkgs/opt/local --host=x86_64-linux 
RUN make && make install

WORKDIR ../

RUN git clone https://github.com/lhorgan/luack
WORKDIR luack
RUN git checkout 43e9e17984e4e992ac2dd0510ac15ebd22f38fdc
RUN make macosx CC=$CC AR="$AR rcu" RANLIB=$RANLIB && make install INSTALL_TOP=/osxcross/target/macports/pkgs/opt/local

WORKDIR ../

RUN git clone https://github.com/metalicjames/selene.git
RUN cp -r selene/include/* /osxcross/target/macports/pkgs/opt/local/include

RUN git clone https://github.com/metalicjames/lua-lz4.git
WORKDIR lua-lz4
RUN LUA_INCDIR=/osxcross/target/macports/pkgs/opt/local/include LUA_LIBDIR=/osxcross/target/macports/pkgs/opt/local/lib make UNAME=Darwin 

WORKDIR ../

RUN git clone https://github.com/auriamg/macdylibbundler.git
WORKDIR macdylibbundler
RUN CXX=clang++ make && make install

WORKDIR ../

RUN wget https://github.com/premake/premake-core/releases/download/v5.0.0-alpha12/premake-5.0.0-alpha12-linux.tar.gz && \
    tar zxvf premake-5.0.0-alpha12-linux.tar.gz && \
    cp premake5 /usr/bin

COPY ./cryptokernel /cryptokernel

RUN cp lua-lz4/lz4.so cryptokernel

RUN git clone https://github.com/metalicjames/cschnorr.git
WORKDIR cschnorr
RUN premake5 gmake2 --os=macosx --include-dir=/osxcross/target/macports/pkgs/opt/local/include && make config=release_static cschnorr && \
    mkdir /osxcross/target/macports/pkgs/opt/local/include/cschnorr/ && \
    cp src/*.h /osxcross/target/macports/pkgs/opt/local/include/cschnorr/ && \
    cp bin/Static/Release/libcschnorr.a /osxcross/target/macports/pkgs/opt/local/lib
WORKDIR ../

WORKDIR cryptokernel
RUN premake5 gmake2 --os=macosx --include-dir=/osxcross/target/macports/pkgs/opt/local/include --lib-dir=/osxcross/target/macports/pkgs/opt/local/lib && \
    make config=release_static ckd

RUN ln -s /osxcross/target/bin/x86_64-apple-darwin15-otool /osxcross/target/bin/otool && ln -s /osxcross/target/bin/x86_64-apple-darwin15-install_name_tool /osxcross/target/bin/install_name_tool && rm -r /opt && ln -s /osxcross/target/macports/pkgs/opt /opt

RUN dylibbundler -b -x bin/Static/Release/ckd -cd -i /usr/lib/ -p @executable_path/libs/
