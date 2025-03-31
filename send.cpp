#include <iostream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define SERVER_IP   "127.0.0.1"
#define SERVER_PORT 12345

int main() {
    std::string xml_content =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<create>\n"
        "<account id=\"123456\" balance=\"1000\"/>\n"
        "<symbol sym=\"SPY\">\n"
        "<account id=\"123456\">100000</account>\n"
        "</symbol>\n"
        "</create>\n";
    
    std::string message = std::to_string(xml_content.size()) + "\n" + xml_content;

    std::cout << message << std::endl;

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

    if (send(sock, message.c_str(), message.size(), 0) < 0) {
        perror("Send failed");
        close(sock);
        return 1;
    }
    
    std::cout << "Message sent successfully." << std::endl;
    char buff[1000];

    while (true) {
        read(sock, buff, 1000);
    }
    return 0;
}