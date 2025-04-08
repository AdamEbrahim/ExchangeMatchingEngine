#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>

#define SERVER_IP   "127.0.0.1"
#define SERVER_PORT 12345

void send_request(int sock, const std::string& request) {
    std::string message = std::to_string(request.size()) + "\n" + request;
    if (send(sock, message.c_str(), message.size(), 0) < 0) {
        perror("Send failed");
        close(sock);
        exit(1);
    }
    //std::cout << "Sent (" << message.size() << " bytes):\n" << request << std::endl;
}

std::string receive_response_get(int sock) {
    char buff[8192];
    int bytes_received = read(sock, buff, sizeof(buff) - 1);
    if (bytes_received > 0) {
        buff[bytes_received] = '\0';
        return std::string(buff);
    }
    return "";
}

std::string extract_order_id(const std::string& response) {
    std::string key = "id=\"";
    size_t start = response.find(key);
    if (start == std::string::npos) return "";
    start += key.length();
    size_t end = response.find("\"", start);
    if (end == std::string::npos) return "";
    return response.substr(start, end - start);
}

void run_client(int sock, std::string aid, int order_count) {
    std::string create_request =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<create>\n"
        "  <account id=\"" + aid + "\" balance=\"500000\"/>\n"
        "  <symbol sym=\"AAPL\">\n"
        "    <account id=\"" + aid + "\">1000</account>\n"
        "  </symbol>\n"
        "</create>\n";

    send_request(sock, create_request);
    receive_response_get(sock);

    srand(time(NULL));

    int remaining_shares = 1000;

    for (int i = 0; i < order_count; i++) {
        int amt = 10 + rand() % 10;
        // if (rand() % 2 == 0) amt *= -1;
        int lim = 95 + rand() % 10;


        if (std::stoi(aid) % 2 == 1) { //sell
            if (amt > remaining_shares) amt = remaining_shares;
            amt *= -1;
            remaining_shares += amt; //subtract
            if (remaining_shares <= 0) break; //no more to sell
        }

        std::string transaction_request =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<transactions id=\"" + aid + "\">\n"
            "  <order sym=\"AAPL\" amount=\"" + std::to_string(amt) +
            "\" limit=\"" + std::to_string(lim) + "\"/>\n"
            "</transactions>\n";

        //buy or sell
        send_request(sock, transaction_request);
        std::string response = receive_response_get(sock);
        std::cout << response << std::endl;
        std::string order_id = extract_order_id(response);

        if (!order_id.empty()) { //no error in response xml
            //every even request should cancel, otherwise query
            if (i % 2 == 0) {
                //cancel
                std::string cancel_req = "<transactions id=\"" + aid + "\"><cancel id=\"" + order_id + "\"/></transactions>";
                send_request(sock, cancel_req);
                std::cout << receive_response_get(sock) << std::endl;
            } else {
                //query
                std::string query_req = "<transactions id=\"" + aid + "\"><query id=\"" + order_id + "\"/></transactions>";
                send_request(sock, query_req);
                std::cout << receive_response_get(sock) << std::endl;
            }

        }

    }

    close(sock);
}

int main(int argc, char * argv[]) {
    if (argc != 3) {
        std::cout << "usage: ./testMixed <account_id> <number_of_requests>\n";
        return EXIT_FAILURE;
    }

    std::string aid = argv[1];
    int request_num = std::stoi(argv[2]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        std::cout << "Socket creation failed" << std::endl;
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        std::cout << "Invalid address"  << std::endl;
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cout << "Connection failed" << std::endl;
        close(sock);
        return 1;
    }

    run_client(sock, aid, request_num);


    return 0;
}
