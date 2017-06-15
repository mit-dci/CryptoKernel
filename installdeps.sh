sudo apt-get update
sudo apt-get install -y git build-essential libjsoncpp-dev libsfml-dev libleveldb-dev libreadline-dev libcurl4-gnutls-dev libmicrohttpd-dev libjsonrpccpp-dev liblua5.3-dev

wget https://www.openssl.org/source/openssl-1.1.0f.tar.gz
tar -xvzf openssl-1.1.0f.tar.gz
cd openssl-1.1.0f
./config
make
sudo make install

cd ../
git clone https://github.com/metalicjames/selene.git
sudo cp -r selene/include/* /usr/local/include

git clone https://github.com/metalicjames/lua-lz4.git
cd lua-lz4
make
sudo cp lz4.so /usr/lib
