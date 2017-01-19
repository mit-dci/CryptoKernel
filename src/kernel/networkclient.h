#ifndef JSONRPC_CPP_STUB_NETWORKCLIENT_H_
#define JSONRPC_CPP_STUB_NETWORKCLIENT_H_

#include <jsonrpccpp/client.h>

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

#endif //JSONRPC_CPP_STUB_NETWORKCLIENT_H_
