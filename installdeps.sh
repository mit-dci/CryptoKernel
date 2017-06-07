apt-get update
apt-get install -y git build-essential libjsoncpp-dev libsfml-dev libleveldb-dev libreadline-dev libcurl4-gnutls-dev libmicrohttpd-dev libjsonrpccpp-dev liblua5.3-dev

wget https://www.openssl.org/source/openssl-1.1.0c.tar.gz
tar -xvzf openssl-1.1.0c.tar.gz
cd openssl-1.1.0c
./config
make
make install

cd ../
git clone https://github.com/metalicjames/selene.git
cp -r selene/include/* /usr/local/include

git clone https://github.com/metalicjames/lua-lz4.git
cd lua-lz4
make
cp lz4.so ../CryptoKernel
