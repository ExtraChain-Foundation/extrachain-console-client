#ifndef RESTAPI_MANAGER_H
    #define RESTAPI_MANAGER_H

    #include "datastorage/blockchain.h"
    #include "managers/extrachain_node.h"
    #include <QObject>
    #include <boost/asio.hpp>
    #include <cpprest/http_listener.h>
    #include <iostream>

using web::http::experimental::listener::http_listener;
using namespace web::http::client;
using namespace web::http;
using namespace web::json;

static std::string numberBlockQuery = "number_block";
static web::http::uri url = "http://127.0.0.1:8085/api/v1";

class ExtraChainNode;

class RestApiManager : public QObject {
    Q_OBJECT
    ExtraChainNode *node;
    http_listener listener;
    boost::asio::io_context handler_context;

public:
    explicit RestApiManager(ExtraChainNode *node, QObject *parent = nullptr);
    ~RestApiManager();

    void countBlocks(http_request request) const;
    void blockContents(int numberBlock, http_request request) const;
    static void interrupt_handler(const boost::system::error_code &error, int signal_number);
    void get(http_request request);
    void runServer();
    void closeServer();

private:
    bool hasQuery(const utility::string_t &query, const std::map<utility::string_t, utility::string_t> &map);
    void initHandlers();
};

#endif // RESTAPI_MANAGER_H
