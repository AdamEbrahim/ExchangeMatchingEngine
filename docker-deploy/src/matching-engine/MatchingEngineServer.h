#ifndef MATCHINGENGINESERVER_H
#define MATCHINGENGINESERVER_H
#include <boost/asio.hpp>
#include "TcpConnection.h"
#include <pqxx/pqxx>

class MatchingEngineServer {
private:
    void start_accept();
    void handle_accept(TcpConnection::ptr connection, const boost::system::error_code& error);

public:
    typedef std::shared_ptr<pqxx::connection> db_ptr;
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    //db_ptr db;


    MatchingEngineServer(boost::asio::io_context& io_context, int port);
    ~MatchingEngineServer();


};

#endif