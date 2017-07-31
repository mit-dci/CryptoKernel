/*************************************************************************
 * libjson-rpc-cpp
 *************************************************************************
 * @file    httpserver.h
 * @date    31.12.2012
 * @author  Peter Spiess-Knafl <dev@spiessknafl.at>, 
 *          James Lovejoy <jameslovejoy1@gmail.com>
 * @license See libjson-rpc-cpp for MIT license
 ************************************************************************/

#ifndef JSONRPC_CPP_HTTPSERVERCONNECTOR_H_
#define JSONRPC_CPP_HTTPSERVERCONNECTOR_H_

#include <stdarg.h>
#include <stdint.h>
#include <sys/types.h>
#if defined(_WIN32) && !defined(__CYGWIN__)
#include <ws2tcpip.h>
#if defined(_MSC_FULL_VER) && !defined (_SSIZE_T_DEFINED)
#define _SSIZE_T_DEFINED
typedef intptr_t ssize_t;
#endif // !_SSIZE_T_DEFINED */
#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#endif

#include <map>
#include <microhttpd.h>
#include <jsonrpccpp/server/abstractserverconnector.h>

namespace jsonrpc
{
    /**
     * This class provides an embedded HTTP Server, based on libmicrohttpd, to handle incoming Requests and send HTTP 1.1
     * valid responses.
     * Note that this class will always send HTTP-Status 200, even though an JSON-RPC Error might have occurred. Please
     * always check for the JSON-RPC Error Header.
     * 
     * The class has been modified from the jsonrpccpp original to only accept connections from
     * localhost.
     */
    class HttpServerLocal : public AbstractServerConnector
    {
        public:

            /**
             * @brief HttpServerLocal, constructor for a modified HttpServer that only accepts connections from localhost
             * @param port on which the server is listening
             * @param enableSpecification - defines if the specification is returned in case of a GET request
             * @param sslcert - defines the path to a SSL certificate, if this path is != "", then SSL/HTTPS is used with the given certificate.
             */
            HttpServerLocal(int port, const std::string& username, const std::string& password, 
							const std::string& sslcert = "", const std::string& sslkey = "", 
							int threads = 1);

            virtual bool StartListening();
            virtual bool StopListening();

            bool virtual SendResponse(const std::string& response,
                    void* addInfo = NULL);
            bool virtual SendOptionsResponse(void* addInfo);

            void SetUrlHandler(const std::string &url, IClientConnectionHandler *handler);

        private:
            int port;
            int threads;
            bool running;
            std::string path_sslcert;
            std::string path_sslkey;
            std::string sslcert;
            std::string sslkey;
			std::string username;
			std::string password;

            struct MHD_Daemon *daemon;

            std::map<std::string, IClientConnectionHandler*> urlhandler;

            static int callback(void *cls, struct MHD_Connection *connection, const char *url, const char *method, const char *version, const char *upload_data, size_t *upload_data_size, void **con_cls);

            IClientConnectionHandler* GetHandler(const std::string &url);

            static int accessCallback(void *cls, const struct sockaddr* addr, socklen_t addrlen);
    };

} /* namespace jsonrpc */
#endif /* JSONRPC_CPP_HTTPSERVERLOCALCONNECTOR_H_ */
