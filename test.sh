g++ -std=c++14 tests/tests.cpp -o test -Isrc -L. -lCryptoKernel -lcrypto -pthread -ldl
./test
