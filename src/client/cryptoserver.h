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

#ifndef JSONRPC_CPP_STUB_CRYPTOSERVER_H_
#define JSONRPC_CPP_STUB_CRYPTOSERVER_H_

#include <jsonrpccpp/server.h>

#include "wallet.h"

class CryptoRPCServer : public jsonrpc::AbstractServer<CryptoRPCServer>
{
    public:
        CryptoRPCServer(jsonrpc::AbstractServerConnector &conn, jsonrpc::serverVersion_t type = jsonrpc::JSONRPC_SERVER_V2) : jsonrpc::AbstractServer<CryptoRPCServer>(conn, type)
        {
            this->bindAndAddMethod(jsonrpc::Procedure("getinfo", jsonrpc::PARAMS_BY_NAME, jsonrpc::JSON_OBJECT,  NULL), &CryptoRPCServer::getinfoI);
            this->bindAndAddMethod(jsonrpc::Procedure("account", jsonrpc::PARAMS_BY_NAME, jsonrpc::JSON_OBJECT, "account",jsonrpc::JSON_STRING, NULL), &CryptoRPCServer::accountI);
            this->bindAndAddMethod(jsonrpc::Procedure("sendtoaddress", jsonrpc::PARAMS_BY_NAME, jsonrpc::JSON_BOOLEAN, "address",jsonrpc::JSON_STRING,"amount",jsonrpc::JSON_REAL,"fee",jsonrpc::JSON_REAL, NULL), &CryptoRPCServer::sendtoaddressI);
            this->bindAndAddMethod(jsonrpc::Procedure("sendrawtransaction", jsonrpc::PARAMS_BY_NAME, jsonrpc::JSON_BOOLEAN, "transaction",jsonrpc::JSON_OBJECT, NULL), &CryptoRPCServer::sendrawtransactionI);
            this->bindAndAddMethod(jsonrpc::Procedure("listaccounts", jsonrpc::PARAMS_BY_NAME, jsonrpc::JSON_OBJECT, NULL), &CryptoRPCServer::listaccountsI);
            this->bindAndAddMethod(jsonrpc::Procedure("listunspentoutputs", jsonrpc::PARAMS_BY_NAME, jsonrpc::JSON_OBJECT, "account",jsonrpc::JSON_STRING, NULL), &CryptoRPCServer::listunspentoutputsI);
        }

        inline virtual void getinfoI(const Json::Value &request, Json::Value &response)
        {
            response = this->getinfo();
        }
        inline virtual void accountI(const Json::Value &request, Json::Value &response)
        {
            response = this->account(request["account"].asString());
        }
        inline virtual void sendtoaddressI(const Json::Value &request, Json::Value &response)
        {
            response = this->sendtoaddress(request["address"].asString(), request["amount"].asDouble(), request["fee"].asDouble());
        }
        inline virtual void sendrawtransactionI(const Json::Value &request, Json::Value &response)
        {
            response = this->sendrawtransaction(request["transaction"]);
        }
        inline virtual void listaccountsI(const Json::Value &request, Json::Value &response)
        {
            response = this->listaccounts();
        }
        inline virtual void listunspentoutputsI(const Json::Value &request, Json::Value &response)
        {
            response = this->listunspentoutputs(request["account"].asString());
        }
        virtual Json::Value getinfo() = 0;
        virtual Json::Value account(const std::string& account) = 0;
        virtual bool sendtoaddress(const std::string& address, double amount, double fee) = 0;
        virtual bool sendrawtransaction(const Json::Value tx) = 0;
        virtual Json::Value listaccounts() = 0;
        virtual Json::Value listunspentoutputs(const std::string& account) = 0;
};

class CryptoServer : public CryptoRPCServer
{
    public:
        CryptoServer(jsonrpc::AbstractServerConnector &connector);

        virtual Json::Value getinfo();
        virtual Json::Value account(const std::string& account);
        virtual bool sendtoaddress(const std::string& address, double amount, double fee);
        virtual bool sendrawtransaction(const Json::Value tx);
        void setWallet(CryptoCurrency::Wallet* Wallet, CryptoKernel::Blockchain* Blockchain, CryptoKernel::Network* Network);
        virtual Json::Value listaccounts();
        virtual Json::Value listunspentoutputs(const std::string& account);

    private:
        CryptoCurrency::Wallet* wallet;
        CryptoKernel::Blockchain* blockchain;
        CryptoKernel::Network* network;
};


#endif //JSONRPC_CPP_STUB_CRYPTOSERVER_H_
