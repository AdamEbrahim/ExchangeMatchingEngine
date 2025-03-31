#ifndef TCPCONNECTION_H
#define TCPCONNECTION_H

#include <boost/asio.hpp>


class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
private:
    TcpConnection(boost::asio::io_context& io_context);

public:
    typedef std::shared_ptr<TcpConnection> ptr;
    boost::asio::ip::tcp::socket socket;
    char buffer[4096]; //buffer to read data into from async_read_some
    std::string message; //holds the total message read over a series of async_read_some

    static ptr create(boost::asio::io_context& io_context);
    void start();

    void handle_write(const boost::system::error_code& error, size_t bytes);
    void handle_read(const boost::system::error_code& error, size_t bytes);

    int parse_message();


};

#endif