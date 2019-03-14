sudo apt-get update && sudo apt-get install -y \
    git \
    build-essential \
    libleveldb-dev \
    libargtable2-dev \
    libreadline-dev \
    libcurl4-gnutls-dev \
    cmake \
    libhiredis-dev \
    doxygen \
    libcppunit-dev \
    libtool \
    automake \
    flex \
    bison

cd /tmp
wget https://www.openssl.org/source/openssl-1.1.0f.tar.gz
tar -xvzf openssl-1.1.0f.tar.gz
cd openssl-1.1.0f
./config
make
sudo make install
sudo ldconfig

cd ../
wget http://ftp.gnu.org/gnu/libmicrohttpd/libmicrohttpd-latest.tar.gz
tar -xvzf libmicrohttpd-latest.tar.gz
rm libmicrohttpd-latest.tar.gz
cd libmicrohttpd*
./configure
make
sudo make install

cd ../
wget https://github.com/open-source-parsers/jsoncpp/archive/1.8.4.tar.gz
tar -xzvf 1.8.4.tar.gz
cd jsoncpp*
cmake .
make
sudo make install

cd ../
wget https://github.com/cinemast/libjson-rpc-cpp/archive/v1.1.0.tar.gz
tar -xvzf v1.1.0.tar.gz
cd libjson-rpc-cpp*
cmake . -DBUILD_SHARED_LIBS=NO -DBUILD_STATIC_LIBS=YES -DCOMPILE_TESTS=NO -DCOMPILE_STUBGEN=NO -DCOMPILE_EXAMPLES=NO
make
sudo make install

cd ../
git clone https://github.com/rweather/noise-c
cd noise-c
./autogen.sh 
./configure
make
sudo make install

cd ../
git clone https://github.com/SFML/SFML.git
cd SFML
cmake . -DBUILD_SHARED_LIBS=NO -DSFML_BUILD_DOC=NO -DSFML_BUILD_AUDIO=NO \
    -DSFML_BUILD_GRAPHICS=NO -DSFML_BUILD_WINDOW=NO -DSFML_BUILD_EXAMPLES=NO \
    -DCMAKE_BUILD_TYPE=Release 
make && sudo make install
sudo cp /usr/local/lib/libsfml-network-s.a /usr/local/lib/libsfml-network.a && sudo cp /usr/local/lib/libsfml-system-s.a /usr/local/lib/libsfml-system.a

cd ../
git clone https://github.com/lhorgan/luack
cd luack
git checkout 43e9e17984e4e992ac2dd0510ac15ebd22f38fdc
make linux && sudo make install
sudo ln -s /usr/local/lib/liblua.a /usr/local/lib/liblua5.3.a

cd ../
git clone https://github.com/metalicjames/selene.git
sudo cp -r selene/include/* /usr/local/include

git clone https://github.com/metalicjames/lua-lz4.git
cd lua-lz4
make
sudo cp lz4.so /usr/lib

cd ../
wget https://github.com/premake/premake-core/releases/download/v5.0.0-alpha13/premake-5.0.0-alpha13-linux.tar.gz && \
tar zxvf premake-5.0.0-alpha13-linux.tar.gz && \
sudo cp premake5 /usr/bin

git clone https://github.com/metalicjames/cschnorr.git
cd cschnorr
premake5 gmake2
make cschnorr
sudo mkdir -p /usr/local/include/cschnorr/
sudo cp src/*.h /usr/local/include/cschnorr/
sudo cp bin/Static/Debug/libcschnorr.a /usr/local/lib
sudo ldconfig
