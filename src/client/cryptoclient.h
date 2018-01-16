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

#ifndef JSONRPC_CPP_STUB_CRYPTOCLIENT_H_
#define JSONRPC_CPP_STUB_CRYPTOCLIENT_H_

#include <jsonrpccpp/client.h>

class CryptoClient : public jsonrpc::Client {
public:
    CryptoClient(jsonrpc::IClientConnector &conn,
                 jsonrpc::clientVersion_t type = jsonrpc::JSONRPC_CLIENT_V2) : jsonrpc::Client(conn,
                             type) {}

    Json::Value getinfo() throw (jsonrpc::JsonRpcException) {
        Json::Value p;
        p = Json::nullValue;
        Json::Value result = this->CallMethod("getinfo",p);
        if (result.isObject())
        { return result; }
        else
        { throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString()); }
    }
    Json::Value account(const std::string& account,
                        const std::string& password) throw (jsonrpc::JsonRpcException) {
        Json::Value p;
        p["account"] = account;
        p["password"] = password;
        Json::Value result = this->CallMethod("account",p);
        if (result.isObject())
        { return result; }
        else
        { throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString()); }
    }
    Json::Value listunspentoutputs(const std::string& account) throw (
        jsonrpc::JsonRpcException) {
        Json::Value p;
        p["account"] = account;
        Json::Value result = this->CallMethod("listunspentoutputs",p);
        if (result.isObject())
        { return result; }
        else
        { throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString()); }
    }
    std::string sendtoaddress(const std::string& address,
                              double amount,
                              const std::string& password) throw (jsonrpc::JsonRpcException) {
        Json::Value p;
        p["address"] = address;
        p["amount"] = amount;
        p["password"] = password;
        Json::Value result = this->CallMethod("sendtoaddress",p);
        if (result.isString())
        { return result.asString(); }
        else
        { throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString()); }
    }
    bool sendrawtransaction(const Json::Value tx) throw (jsonrpc::JsonRpcException) {
        Json::Value p;
        p["transaction"] = tx;
        Json::Value result = this->CallMethod("sendrawtransaction",p);
        if (result.isBool())
        { return result.asBool(); }
        else
        { throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString()); }
    }
    Json::Value listaccounts() throw (jsonrpc::JsonRpcException) {
        Json::Value p;
        p = Json::nullValue;
        Json::Value result = this->CallMethod("listaccounts",p);
        if (result.isObject())
        { return result; }
        else
        { throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString()); }
    }
    std::string compilecontract(const std::string& code) throw (jsonrpc::JsonRpcException) {
        Json::Value p;
        p["code"] = code;
        Json::Value result = this->CallMethod("compilecontract",p);
        if (result.isString())
        { return result.asString(); }
        else
        { throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString()); }
    }
    std::string calculateoutputid(const Json::Value output) throw (
        jsonrpc::JsonRpcException) {
        Json::Value p;
        p["output"] = output;
        Json::Value result = this->CallMethod("calculateoutputid",p);
        if (result.isString())
        { return result.asString(); }
        else
        { throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString()); }
    }
    Json::Value signtransaction(const Json::Value& tx,
                                const std::string& password) throw (jsonrpc::JsonRpcException) {
        Json::Value p;
        p["transaction"] = tx;
        p["password"] = password;
        Json::Value result = this->CallMethod("signtransaction",p);
        if (result.isObject())
        { return result; }
        else
        { throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString()); }
    }
    Json::Value listtransactions() throw (jsonrpc::JsonRpcException) {
        Json::Value p;
        p = Json::nullValue;
        Json::Value result = this->CallMethod("listtransactions",p);
        if (result.isObject())
        { return result; }
        else
        { throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString()); }
    }
    Json::Value getblockbyheight(const uint64_t height) throw (jsonrpc::JsonRpcException) {
        Json::Value p;
        p["height"] = height;
        const Json::Value result = this->CallMethod("getblockbyheight", p);
        if (result.isObject()) {
            return result;
        } else {
            throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE,
                                            result.toStyledString());
        }
    }
    Json::Value stop() throw (jsonrpc::JsonRpcException) {
        const Json::Value result = this->CallMethod("stop", Json::nullValue);
        if (result.isBool()) {
            return result;
        } else {
            throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE,
                                            result.toStyledString());
        }
    }
    Json::Value getblock(const std::string& id) throw (jsonrpc::JsonRpcException) {
        Json::Value p;
        p["id"] = id;
        const Json::Value result = this->CallMethod("getblock", p);
        if (result.isObject()) {
            return result;
        } else {
            throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE,
                                            result.toStyledString());
        }
    }
    Json::Value gettransaction(const std::string& id) throw (jsonrpc::JsonRpcException) {
        Json::Value p;
        p["id"] = id;
        const Json::Value result = this->CallMethod("gettransaction", p);
        if (result.isObject()) {
            return result;
        } else {
            throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE,
                                            result.toStyledString());
        }
    }
    Json::Value importprivkey(const std::string& name,
                              const std::string& key,
                              const std::string& password) throw (jsonrpc::JsonRpcException) {
        Json::Value p;
        p["name"] = name;
        p["key"] = key;
        p["password"] = password;
        const Json::Value result = this->CallMethod("importprivkey", p);
        if (result.isObject()) {
            return result;
        } else {
            throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE,
                                            result.toStyledString());
        }
    }
    Json::Value getpeerinfo() throw (jsonrpc::JsonRpcException) {
       Json::Value p;
        p = Json::nullValue;
        Json::Value result = this->CallMethod("getpeerinfo",p);
        if (result.isObject())
        { return result; }
        else
        { throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString()); }
    }
    Json::Value dumpprivkeys(const std::string& account,
                             const std::string& password) throw (jsonrpc::JsonRpcException) {
        Json::Value p;
        p["account"] = account;
        p["password"] = password;
        Json::Value result = this->CallMethod("dumpprivkeys",p);
        if (result.isObject())
        { return result; }
        else
        { throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString()); }
    }
};

#endif //JSONRPC_CPP_STUB_CRYPTOCLIENT_H_
