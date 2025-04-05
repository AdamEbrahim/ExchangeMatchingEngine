#include "MatchingEngineServer.h"
#include "TcpConnection.h"
#include <iostream>
#include "DatabaseTransactions.h"
#include <stdexcept>

MatchingEngineServer::MatchingEngineServer(boost::asio::io_context& io_context, int port) : io_context_(io_context), acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)) {
    start_accept(); //only 1 thread calls, start accepting connections
}

MatchingEngineServer::~MatchingEngineServer() {

}

//start accepting connections on port
void MatchingEngineServer::start_accept() {
    TcpConnection::ptr new_connection = TcpConnection::create(io_context_);

    boost::asio::ip::tcp::socket& sock = new_connection->socket;
    // acceptor_.async_accept(sock, //async task, 1 thread in pool will call completion handler
    //                     std::bind(&MatchingEngineServer::handle_accept, this, new_connection, boost::asio::placeholders::error)); 
    acceptor_.async_accept(sock, 
        [this, new_connection](const boost::system::error_code& error) {
            handle_accept(new_connection, error);
        }
    );
}

//a thread from pool should be dispatched to handle accepting new connection
void MatchingEngineServer::handle_accept(TcpConnection::ptr connection, const boost::system::error_code& error) {
    std::cout << "got new connection" << std::endl;
    if (!error) {
        connection->start();
    }

    start_accept(); //only 1 thread calls this (the current one handling the prev async_accept), so only one async_accept() setup
}