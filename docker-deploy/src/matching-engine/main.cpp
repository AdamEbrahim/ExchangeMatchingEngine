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
        MatchingEngineServer server(); //constructor will call start_accept and set up async tasks
        //create thread pool and call io_context.run()
        std::vector<std::thread> threads (THREAD_POOL_SIZE-1, std::thread())

    } catch (const std::exception& e) {
        std::cout << "Exception caught: " << e.what() << std::endl << "Shutting down server..." << std::endl;
    }

    return 0;
}