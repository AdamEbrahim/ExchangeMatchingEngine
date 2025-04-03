#include <iostream>
#include <string>
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
    // std::cout << "Sent:\n" << message << std::endl;
}

void receive_response(int sock) {
    char buff[1000];
    int bytes_received = read(sock, buff, 1000);
    if (bytes_received > 0) {
        buff[bytes_received] = '\0';
        // std::cout << "Received:\n" << buff << std::endl;
    }
}

int main(int argc, char * argv[]) {
    if (argc != 3) {
        std::cout << "usage: ./test <account _id> <number of requests>" << std::endl;
        return EXIT_FAILURE;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return 1;
    }

    //Test Create Account
    std::string aid = argv[1];
    std::string create_request =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<create>\n"
        "<account id=\"" + aid + "\" balance=\"5000\"/>\n"
        "<symbol sym=\"AAPL\">\n"
        "<account id=\"" + aid + "\">50</account>\n"
        "</symbol>\n"
        "</create>\n";
    send_request(sock, create_request);
    receive_response(sock);

    // Test Transactions (Order and Query)
    int request_num = std::stoi(argv[2]);
    int amt;
    int lim;
    for (int i = 0; i < request_num; i++) {
        amt = 10;
        amt += rand() % 10;
        if (rand() % 2 == 0) {
            amt *= -1;
        }

        lim = 95;
        lim += rand() % 10;
        std::string transaction_request =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<transactions id=\"" + aid + "\">\n"
            "<order sym=\"AAPL\" "
            "amount=\"" + std::to_string(amt) + "\" limit=\"" + std::to_string(lim) + "\"/>\n"
            "</transactions>\n";
        send_request(sock, transaction_request);
        receive_response(sock);
    } 
    
    close(sock);
    return 0;
}