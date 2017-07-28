sudo apt-get update
sudo apt-get install -y git build-essential libjsoncpp-dev libsfml-dev libleveldb-dev libargtable2-dev libreadline-dev libcurl4-gnutls-dev liblua5.3-dev cmake

wget https://www.openssl.org/source/openssl-1.1.0f.tar.gz
tar -xvzf openssl-1.1.0f.tar.gz
cd openssl-1.1.0f
./config
make
sudo make install
cd ../

wget http://ftp.gnu.org/gnu/libmicrohttpd/libmicrohttpd-latest.tar.gz
tar -xvzf libmicrohttpd-latest.tar.gz
cd libmicrohttpd*
./configure
make
sudo make install
cd ../

wget https://github.com/cinemast/libjson-rpc-cpp/archive/v0.7.0.tar.gz
tar -xvzf v0.7.0.tar.gz
cd libjson-rpc-cpp*
cmake .
make
sudo make install

cd ../
git clone https://github.com/metalicjames/selene.git
sudo cp -r selene/include/* /usr/local/include

git clone https://github.com/metalicjames/lua-lz4.git
cd lua-lz4
make
sudo cp lz4.so /usr/lib
