#ifndef JSONRPC_CPP_STUB_NETWORKCLIENT_H_
#define JSONRPC_CPP_STUB_NETWORKCLIENT_H_

#include <memory>

#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/connectors/httpclient.h>

#include "network.h"
#include "blockchain.h"

class NetworkClient : public jsonrpc::Client
{
    public:
        NetworkClient(jsonrpc::IClientConnector &conn, jsonrpc::clientVersion_t type = jsonrpc::JSONRPC_CLIENT_V2) : jsonrpc::Client(conn, type) {}

        Json::Value getblock(int height, const std::string& id) throw (jsonrpc::JsonRpcException)
        {
            Json::Value p;
            p["height"] = height;
            p["id"] = id;
            Json::Value result = this->CallMethod("getblock",p);
            if (result.isObject())
                return result;
            else
                throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString());
        }
        Json::Value getblocks(int end, int start) throw (jsonrpc::JsonRpcException)
        {
            Json::Value p;
            p["end"] = end;
            p["start"] = start;
            Json::Value result = this->CallMethod("getblocks",p);
            if (result.isObject())
                return result;
            else
                throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString());
        }
        Json::Value getunconfirmed() throw (jsonrpc::JsonRpcException)
        {
            Json::Value p;
            p = Json::nullValue;
            Json::Value result = this->CallMethod("getunconfirmed",p);
            if (result.isObject())
                return result;
            else
                throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString());
        }
        void block(const Json::Value& data) throw (jsonrpc::JsonRpcException)
        {
            Json::Value p;
            p["data"] = data;
            this->CallNotification("block",p);
        }
        void transactions(const Json::Value& data) throw (jsonrpc::JsonRpcException)
        {
            Json::Value p;
            p["data"] = data;
            this->CallNotification("transactions",p);
        }
        Json::Value info() throw (jsonrpc::JsonRpcException)
        {
            Json::Value p;
            p = Json::nullValue;
            Json::Value result = this->CallMethod("info",p);
            if (result.isObject())
                return result;
            else
                throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString());
        }
};

class CryptoKernel::Network::Client
{
    public:
        Client(const std::string url);
        ~Client();

        Json::Value getInfo();
        void sendTransactions(const std::vector<CryptoKernel::Blockchain::transaction> transactions);
        void sendBlock(const CryptoKernel::Blockchain::block block);
        std::vector<CryptoKernel::Blockchain::transaction> getUnconfirmedTransactions();
        CryptoKernel::Blockchain::block getBlock(const uint64_t height, const std::string id);
        std::vector<CryptoKernel::Blockchain::block> getBlocks(const int start, const int end);

    private:
        std::unique_ptr<jsonrpc::HttpClient> httpClient;
        std::unique_ptr<NetworkClient> client;
};

#endif //JSONRPC_CPP_STUB_NETWORKCLIENT_H_
