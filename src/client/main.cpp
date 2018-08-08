/*  CryptoCurrency - An example crypto-currency using the CryptoKernel library as a base
    Copyright (C) 2016  James Lovejoy

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <thread>
#include <iostream>
#include <stdlib.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <csignal>

#include <jsonrpccpp/client/connectors/httpclient.h>

#include <openssl/rand.h>

#include "ckmath.h"
#include "crypto.h"
#include "wallet.h"
#include "network.h"
#include "cryptoclient.h"
#include "version.h"
#include "consensus/PoW.h"
#include "base64.h"
#include "multicoin.h"

bool running;

int main(int argc, char* argv[]) {
    std::ifstream t("config.json");
    if(!t.is_open()) {
        throw std::runtime_error("Could not open config file");
    }

    std::string buffer((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
    Json::Value config = CryptoKernel::Storage::toJson(buffer);

    t.close();

    if(config["rpcpassword"] == "password") {
        unsigned char bytes[32];
        if(!RAND_bytes((unsigned char*)&bytes, 32)) {
            throw std::runtime_error("Couldn't generate random rpc password");
        }

        config["rpcpassword"] = base64_encode((unsigned char*)&bytes, 32);

        std::ofstream ofs("config.json", std::ofstream::out | std::ofstream::trunc);

        ofs << config.toStyledString();

        ofs.close();
    }

    if(argc < 2) {
        CryptoKernel::Log log("CryptoKernel.log", config["verbose"].asBool());

        running = true;
        std::signal(SIGINT, [](int signal) { running = false; });

        CryptoKernel::MulticoinLoader loader("./config.json", &log, &running);

        if(!config["verbose"].asBool()) {
            std::cout << "ck daemon started" << std::endl;
        }

        while(running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        log.printf(LOG_LEVEL_INFO, "Shutting down...");
    } else {
        std::string command(argv[1]);

        std::string port = "8383";

        int offset = 0;

        if(command == "-p" && argc >= 4) {
            command = std::string(argv[3]);
            port = std::string(argv[2]);
            offset = 2;
        } else if(command == "-p") {
            throw std::runtime_error("Malformed commands");
        }

		const std::string userpass = config["rpcuser"].asString()
                                   + ":"
                                   + config["rpcpassword"].asString();
		const std::string auth = base64_encode((unsigned char*)userpass.c_str(),
		                                       userpass.size());

        jsonrpc::HttpClient httpclient("http://127.0.0.1:" + port);
        httpclient.SetTimeout(30000);
		httpclient.AddHeader("Authorization", "Basic " + auth);
        CryptoClient client(httpclient);

        try {
            if(command == "-daemon") {
                #ifdef _WIN32
                if(std::system("start /B .\\ckd") != 0) {
                #else
                if(std::system("./ckd &") != 0) {
                #endif
                    std::cout << "Failed to start ckd" << std::endl;
                }
            } else if(command == "getinfo") {
                std::cout << client.getinfo().toStyledString() << std::endl;
            } else if(command == "account") {
                if(argc >= 3 + offset) {
                    const std::string name(argv[2 + offset]);
                    const std::string password = getPass("Please enter your wallet passphrase: ");
                    std::cout << client.account(name, password).toStyledString() << std::endl;
                } else {
                    std::cout << "Usage: account [accountname]" << std::endl;
                }
            } else if(command == "sendtoaddress") {
                if(argc >= 4 + offset) {
                    const std::string address(argv[2 + offset]);
                    const double amount(std::strtod(argv[3 + offset], NULL));
                    const std::string password = getPass("Please enter your wallet passphrase: ");
                    std::cout << client.sendtoaddress(address, amount, password) << std::endl;
                } else {
                    std::cout << "Usage: sendtoaddress [address] [amount]" << std::endl;
                }
            } else if(command == "listaccounts") {
                std::cout << client.listaccounts().toStyledString() << std::endl;
            } else if(command == "listunspentoutputs") {
                if(argc == 3 + offset) {
                    std::string name(argv[2 + offset]);
                    std::cout << client.listunspentoutputs(name).toStyledString() << std::endl;
                } else {
                    std::cout << "Usage: listunspentoutputs [accountname]" << std::endl;
                }
            } else if(command == "compilecontract") {
                if(argc == 3 + offset) {
                    std::string code(argv[2 + offset]);
                    std::cout << client.compilecontract(code) << std::endl;
                } else {
                    std::cout << "Usage: compilecontract [code]" << std::endl;
                }
            } else if(command == "listtransactions") {
                std::cout << client.listtransactions().toStyledString() << std::endl;
            } else if(command == "getblock") {
                if(argc == 3 + offset) {
                    std::cout << client.getblock(std::string(argv[2 + offset])) << std::endl;
                } else {
                    std::cout << "Usage: getblock [id]" << std::endl;
                }
            } else if(command == "getblockbyheight") {
                if(argc == 3 + offset) {
                    std::cout << client.getblockbyheight(std::strtoull(argv[2 + offset], nullptr,
                                                         0)).toStyledString() << std::endl;
                } else {
                    std::cout << "Usage: getblockbyheight [height]" << std::endl;
                }
            } else if(command == "stop") {
                std::cout << client.stop().toStyledString() << std::endl;
            } else if(command == "importprivkey") {
                if(argc >= 4 + offset) {
                    const std::string password = getPass("Please enter your wallet passphrase: ");
                    std::cout << client.importprivkey(std::string(argv[2 + offset]), std::string(argv[3 + offset]),
                                                      password);
                } else {
                    std::cout << "Usage: importprivkey [accountname] [privkey]" << std::endl;
                }
            } else if(command == "getpeerinfo") {
                std::cout << client.getpeerinfo() << std::endl;
            } else if(command == "gettransaction") {
                if(argc == 3 + offset) {
                    std::cout << client.gettransaction(std::string(argv[2 + offset])).toStyledString() << std::endl;
                } else {
                    std::cout << "Usage: gettransaction [id]" << std::endl;
                }
            } else if(command == "dumpprivkeys") {
                if(argc >= 3 + offset) {
                    const std::string name(argv[2 + offset]);
                    const std::string password = getPass("Please enter your wallet passphrase: ");
                    std::cout << client.dumpprivkeys(name, password).toStyledString() << std::endl;
                } else {
                    std::cout << "Usage: dumpprivkeys [accountname]" << std::endl;
                }
            } else {
                std::cout << "CryptoKernel - Blockchain Development Toolkit - v" << version << "\n\n"
                          << "[-p [port]]\n\n"
                          << "account [accountname]\n"
                          << "compilecontract [code]\n"
                          << "dumpprivkeys [accountname]\n"
                          << "getblock [id]\n"
                          << "getblockbyheight [height]\n"
                          << "getinfo\n"
                          << "getpeerinfo\n"
                          << "gettransaction [id]\n"
                          << "importprivkey [accountname] [privkey]\n"
                          << "listaccounts\n"
                          << "listtransactions\n"
                          << "listunspentoutputs [accountname]\n"
                          << "sendtoaddress [address] [amount]\n"
                          << "stop\n";
            }
        } catch(jsonrpc::JsonRpcException e) {
            std::cout << e.what() << std::endl;
        }
    }

    return 0;
}
