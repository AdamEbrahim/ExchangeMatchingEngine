//entry point
#include "MatchingEngineServer.h"
#include <exception>
#include <iostream>
#include <thread>
#include <vector>
#include <boost/asio.hpp>
#include "DatabaseTransactions.h"

#define THREAD_POOL_SIZE 8
#define SERVER_PORT 12345


thread_local std::shared_ptr<pqxx::connection> thread_conn;

int main() {
    std::cout << "PID: " << getpid() << std::endl;

    try {
        boost::asio::io_context io_context;

        //create pool of db connection pointers, retrying for each connection if needed
        std::vector<std::shared_ptr<pqxx::connection>> connection_pool;
        for (int i = 0; i < THREAD_POOL_SIZE; ++i) {
            connection_pool.push_back([](){
                int connection_attempt = 0;
                while (connection_attempt < 11) {
                    try {
                        return std::make_shared<pqxx::connection>(
                            "dbname=postgres user=postgres password=postgres host=db port=5432");
                    } catch (const std::exception& e) {
                        connection_attempt++;
                        if (connection_attempt == 10) {
                            throw;
                        }
                        std::this_thread::sleep_for(std::chrono::seconds(1)); //1 second sleep of thread
                    }
                }
            }()); //add () to call the lambda
        }

        //main thread gets 0th connection, resets db
        thread_conn = connection_pool[0];
        DatabaseTransactions::setup();

        MatchingEngineServer server(io_context, SERVER_PORT); //constructor will call start_accept and set up async tasks/work


        //thread pool
        std::vector<std::thread> threads;
        for (int i = 1; i < THREAD_POOL_SIZE; i++) {
            threads.emplace_back([&]{ 
                thread_conn = connection_pool[i]; //setting the thread local db connection variable (top of file)
                io_context.run(); 
            });
        }

        //main thread
        io_context.run(); 

        //if a thread has an exception during io_context.run(), should handle it inside so we don't go to this
        //exception handler. For example, just make thread drop the excepting task.

        std::cout << "Joining threads and shutting down server..." << std::endl;
        for (std::thread& a :threads) {
            if (a.joinable()) {
                a.join();
            }
        }

    } catch (const std::exception& e) {
        std::cout << "Exception caught: " << e.what() << std::endl << "Shutting down server..." << std::endl;
    }

    return 0;
}