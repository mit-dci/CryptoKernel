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
        }

        inline virtual void getinfoI(const Json::Value &request, Json::Value &response)
        {
            (void)request;
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
        virtual Json::Value getinfo() = 0;
        virtual Json::Value account(const std::string& account) = 0;
        virtual bool sendtoaddress(const std::string& address, double amount, double fee) = 0;
};

class CryptoServer : public CryptoRPCServer
{
    public:
        CryptoServer(jsonrpc::AbstractServerConnector &connector);

        virtual Json::Value getinfo();
        virtual Json::Value account(const std::string& account);
        virtual bool sendtoaddress(const std::string& address, double amount, double fee);
        void setWallet(CryptoCurrency::Wallet* Wallet, CryptoCurrency::Protocol* Protocol, CryptoKernel::Blockchain* Blockchain);

    private:
        CryptoCurrency::Wallet* wallet;
        CryptoCurrency::Protocol* protocol;
        CryptoKernel::Blockchain* blockchain;
};


#endif //JSONRPC_CPP_STUB_CRYPTOSERVER_H_
