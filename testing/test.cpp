#include <iostream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>
#include <time.h>
#include <vector>

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

void receive_response(int sock) {
    char buff[4096];
    int bytes_received = read(sock, buff, sizeof(buff) - 1);
    if (bytes_received > 0) {
        buff[bytes_received] = '\0';
        //std::cout << "Received (" << bytes_received << " bytes):\n" << buff << std::endl;
    } else {
        std::cerr << "No response received or read failed.\n";
    }
    //std::cout << "--------------------------------------\n";
}

int main(int argc, char * argv[]) {
    if (argc != 3) {
        std::cout << "usage: ./test <account_id> <number_of_requests>\n";
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

    std::vector<std::string> stocks = {"AAPL", "GOOG", "NVDA", "AMZN", "MSFT", "SBUX", "DIS", "GE"};
    //create account and add shares
    std::string create_request =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<create>\n"
        "  <account id=\"" + aid + "\" balance=\"500000\"/>\n"
        "  <symbol sym=\"AAPL\">\n"
        "    <account id=\"" + aid + "\">1000</account>\n"
        "  </symbol>\n"
        "  <symbol sym=\"GOOG\">\n"
        "    <account id=\"" + aid + "\">1000</account>\n"
        "  </symbol>\n"
        "  <symbol sym=\"NVDA\">\n"
        "    <account id=\"" + aid + "\">1000</account>\n"
        "  </symbol>\n"
        "  <symbol sym=\"AMZN\">\n"
        "    <account id=\"" + aid + "\">1000</account>\n"
        "  </symbol>\n"
        "  <symbol sym=\"MSFT\">\n"
        "    <account id=\"" + aid + "\">1000</account>\n"
        "  </symbol>\n"
        "  <symbol sym=\"SBUX\">\n"
        "    <account id=\"" + aid + "\">1000</account>\n"
        "  </symbol>\n"
        "  <symbol sym=\"DIS\">\n"
        "    <account id=\"" + aid + "\">1000</account>\n"
        "  </symbol>\n"
        "  <symbol sym=\"GE\">\n"
        "    <account id=\"" + aid + "\">1000</account>\n"
        "  </symbol>\n"
        "</create>\n";

    send_request(sock, create_request);
    receive_response(sock);

    srand(time(NULL));

    int remaining_shares = 1000;
    //send transaction requests
    for (int i = 0; i < request_num; i++) {
        int amt = 10 + rand() % 10;
        // if (rand() % 2 == 0) amt *= -1;
        int lim = 95 + rand() % 10;

        std::string stock = stocks[rand() % stocks.size()];


        if (std::stoi(aid) % 2 == 1) { //sell
            if (amt > remaining_shares) amt = remaining_shares;
            amt *= -1;
            remaining_shares += amt; //subtract
            if (remaining_shares <= 0) break; //no more to sell
        }

        std::string transaction_request =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<transactions id=\"" + aid + "\">\n"
            "  <order sym=\"" + stock + "\" amount=\"" + std::to_string(amt) +
            "\" limit=\"" + std::to_string(lim) + "\"/>\n"
            "</transactions>\n";

        send_request(sock, transaction_request);
        receive_response(sock);
    }

    close(sock);
    return 0;
}