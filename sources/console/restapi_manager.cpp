#include "console/restapi_manager.h"

RestApiManager::RestApiManager(ExtraChainNode *node, QObject *parent)
    : QObject(parent)
    , node(node)
    , listener(url) {
    initHandlers();
}

RestApiManager::~RestApiManager() {
//    listener.close().wait();
}

void RestApiManager::countBlocks(http_request request) const {
    const auto length = node->blockchain()->getBlockChainLength();
    value response;
    response[U("status")] = value::number(status_codes::OK);
    response[U("count_blocks")] = value::string(length.toStdString());
    request.reply(status_codes::OK, response);
}

void RestApiManager::blockContents(int numberBlock, http_request request) const {
    const Block block = node->blockchain()->getBlock(
        SearchEnum::BlockParam::Id, QByteArray::fromStdString(BigNumber(numberBlock).toStdString()));

    value resp;
    resp[U("status")] = value::number(status_codes::OK);
    resp[U("block_hash")] = value::string(block.getHash());
    resp[U("previous_block_hash")] = value::string(block.getPrevHash());
    resp[U("type")] = value::string(block.getType());
    resp[U("date")] = value::number(block.getDate());
    resp[U("index")] = value::number(std::stoi(block.getIndex().toStdString()));
    resp[U("approver")] = value::string(block.getApprover().toStdString());
    resp[U("data")] = value::string(block.getData());

    request.reply(status_codes::OK, resp);
}

void RestApiManager::interrupt_handler(const boost::system::error_code &error, int signal_number) {
    //...
}

void RestApiManager::get(http_request request) {
    std::cout << "Call get request..." << std::endl;
    auto response = request.extract_json().get();
    const std::map<utility::string_t, utility::string_t> queries =
        request.request_uri().split_query(request.request_uri().query());

    if (queries.empty()) {
        countBlocks(request);
    } else if (hasQuery(numberBlockQuery, queries)) {
        blockContents(std::atoi(queries.find(numberBlockQuery)->second.c_str()), request);
    } else {
        request.reply(status_codes::OK, "Bad query or not exist.");
    }
}

void RestApiManager::runServer() {
    try {
        listener.open().then([] { std::cout << "listening..." << std::endl; }).wait();
    } catch (std::exception const &e) {
        std::cout << "Error: " << e.what() << std::endl;
    }

    boost::asio::signal_set Signals(handler_context, SIGINT);
    Signals.async_wait(interrupt_handler);
    handler_context.run();
}

void RestApiManager::closeServer() {
    listener.close().wait();
}

bool RestApiManager::hasQuery(const utility::string_t &query,
                              const std::map<utility::string_t, utility::string_t> &map) {
    return !map.find(query)->first.empty();
}

void RestApiManager::initHandlers() {
    listener.support(methods::GET, std::bind(&RestApiManager::get, this, std::placeholders::_1));
}
