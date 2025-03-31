//entry point
#include "MatchingEngineServer.h"
#include <exception>
#include <iostream>
#include <thread>
#include <vector>
#include <boost/asio.hpp>

#define THREAD_POOL_SIZE 8
#define SERVER_PORT 12345

int main() {
    try {
        boost::asio::io_context io_context;

        MatchingEngineServer server(io_context, SERVER_PORT); //constructor will call start_accept and set up async tasks/work

        //thread pool
        std::vector<std::thread> threads;
        for (int i = 0; i < THREAD_POOL_SIZE - 1; i++) {
            threads.emplace_back([&]{ io_context.run(); });
        }

        io_context.run(); //main thread
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