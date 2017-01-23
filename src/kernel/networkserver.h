#ifndef JSONRPC_CPP_STUB_NETWORKSERVER_H_
#define JSONRPC_CPP_STUB_NETWORKSERVER_H_

#include <jsonrpccpp/server.h>

#include "blockchain.h"
#include "network.h"

class NetworkServer : public jsonrpc::AbstractServer<NetworkServer>
{
    public:
        NetworkServer(jsonrpc::AbstractServerConnector &conn, jsonrpc::serverVersion_t type = jsonrpc::JSONRPC_SERVER_V2) : jsonrpc::AbstractServer<NetworkServer>(conn, type)
        {
            this->bindAndAddMethod(jsonrpc::Procedure("getblock", jsonrpc::PARAMS_BY_NAME, jsonrpc::JSON_OBJECT, "height",jsonrpc::JSON_INTEGER,"id",jsonrpc::JSON_STRING, NULL), &NetworkServer::getblockI);
            this->bindAndAddMethod(jsonrpc::Procedure("getblocks", jsonrpc::PARAMS_BY_NAME, jsonrpc::JSON_OBJECT, "end",jsonrpc::JSON_INTEGER,"start",jsonrpc::JSON_INTEGER, NULL), &NetworkServer::getblocksI);
            this->bindAndAddMethod(jsonrpc::Procedure("getunconfirmed", jsonrpc::PARAMS_BY_NAME, jsonrpc::JSON_OBJECT,  NULL), &NetworkServer::getunconfirmedI);
            this->bindAndAddNotification(jsonrpc::Procedure("block", jsonrpc::PARAMS_BY_NAME, "data",jsonrpc::JSON_OBJECT, NULL), &NetworkServer::blockI);
            this->bindAndAddNotification(jsonrpc::Procedure("transactions", jsonrpc::PARAMS_BY_NAME, "data",jsonrpc::JSON_OBJECT, NULL), &NetworkServer::transactionsI);
            this->bindAndAddMethod(jsonrpc::Procedure("info", jsonrpc::PARAMS_BY_NAME, jsonrpc::JSON_OBJECT,  NULL), &NetworkServer::infoI);
        }

        inline virtual void getblockI(const Json::Value &request, Json::Value &response)
        {
            response = this->getblock(request["height"].asInt(), request["id"].asString());
        }
        inline virtual void getblocksI(const Json::Value &request, Json::Value &response)
        {
            response = this->getblocks(request["end"].asInt(), request["start"].asInt());
        }
        inline virtual void getunconfirmedI(const Json::Value &request, Json::Value &response)
        {
            (void)request;
            response = this->getunconfirmed();
        }
        inline virtual void blockI(const Json::Value &request)
        {
            this->block(request["data"]);
        }
        inline virtual void transactionsI(const Json::Value &request)
        {
            this->transactions(request["data"]);
        }
        inline virtual void infoI(const Json::Value &request, Json::Value &response)
        {
            (void)request;
            response = this->info();
        }
        virtual Json::Value getblock(int height, const std::string& id) = 0;
        virtual Json::Value getblocks(int end, int start) = 0;
        virtual Json::Value getunconfirmed() = 0;
        virtual void block(const Json::Value& data) = 0;
        virtual void transactions(const Json::Value& data) = 0;
        virtual Json::Value info() = 0;
};

class CryptoKernel::Network::Server : public NetworkServer
{
    public:
        Server(jsonrpc::AbstractServerConnector &connector);

        virtual Json::Value getblock(const int height, const std::string& id);
        virtual Json::Value getblocks(const int end, const int start);
        virtual Json::Value getunconfirmed();
        virtual void block(const Json::Value& data);
        virtual void transactions(const Json::Value& data);
        virtual Json::Value info();

        void setBlockchain(CryptoKernel::Blockchain* blockchain);
        void setNetwork(CryptoKernel::Network* network);

    private:
        CryptoKernel::Blockchain* blockchain;
        CryptoKernel::Network* network;
};

#endif //JSONRPC_CPP_STUB_NETWORKSERVER_H_
