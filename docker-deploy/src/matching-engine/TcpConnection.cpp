#include "TcpConnection.h"
#include <iostream>
#include <sstream>
#include "tinyxml2.h"
#include <vector>
#include "DatabaseTransactions.h"

TcpConnection::TcpConnection(boost::asio::io_context& io_context, db_ptr db) : socket(io_context), C(db) {

}

TcpConnection::ptr TcpConnection::create(boost::asio::io_context& io_context, db_ptr db) {
    return TcpConnection::ptr(new TcpConnection(io_context, db)); //shared ptr
}

void TcpConnection::start() {
    auto self = shared_from_this(); //creates new shared_ptr to increase ref count until async_read finishes

    //async task to read from a socket, once gets data over socket will dispatch a thread to do completion handler
    socket.async_read_some(boost::asio::buffer(buffer),
        [self](const boost::system::error_code& error, size_t bytes) {self->handle_read(error, bytes);});
}

void TcpConnection::handle_read(const boost::system::error_code& error, size_t bytes) {
    if (error) {
        std::cout << error.message() << std::endl;
        return;
    }

    message += std::string(buffer, bytes);

    //do some computation on the message
    if (parse_message() < 0) {
        auto self = shared_from_this(); //creates new shared_ptr to increase ref count until async_read finishes

        //async task to read from a socket, once gets data over socket will dispatch a thread to do completion handler
        socket.async_read_some(boost::asio::buffer(buffer),
            [self](const boost::system::error_code& error, size_t bytes) {self->handle_read(error, bytes);});
        return;

    }
    //have full message

    auto self = shared_from_this(); //creates new shared_ptr to increase ref count until async_read finishes

    //async task to read from a socket, once gets data over socket will dispatch a thread to do completion handler
    socket.async_read_some(boost::asio::buffer(buffer),
        [self](const boost::system::error_code& error, size_t bytes) {self->handle_read(error, bytes);});
    return;

}

//return -1 if incomplete message
int TcpConnection::parse_message() {
    if (message.find("\n") == std::string::npos) {
        return -1;
    }

    std::istringstream message_stream(message);
    std::string line;

    std::getline(message_stream, line);
    uint32_t xml_len = std::stoul(line);
    int xml_start = line.length() + 1; //index of start of xml

    if (xml_start + xml_len < message.length()) { //haven't received full XML
        std::cout << "haven received full xml" << std::endl;
        return -1;
    }
    std::cout << "have received full xml, " << message.length() << std::endl;
    
    //extract the actual xml, so remove anything like "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    std::getline(message_stream, line);
    if (line.find("xml version") != std::string::npos) {
        xml_start += line.length() + 1;
    }


    tinyxml2::XMLDocument doc;
    tinyxml2::XMLError eResult = doc.Parse(message_stream.str().substr(xml_start).c_str());

    if (eResult != tinyxml2::XML_SUCCESS) {
        std::cout << "Error parsing XML: " << doc.ErrorStr() << std::endl;
        return -1;
    }

    tinyxml2::XMLNode* root = doc.FirstChildElement();
    if (root == nullptr) {
        std::cout << "Root element not found" << std::endl;
        return -1;
    }

    if (std::string(root->Value()) == "create") {
        for (tinyxml2::XMLElement* element = root->FirstChildElement(); element != nullptr; element = element->NextSiblingElement()) {

            if (std::string(element->Value()) == "account") {
                uint32_t id = 0;
                float balance = 0;
                element->QueryUnsignedAttribute("id", &id);
                element->QueryFloatAttribute("balance", &balance);

                std::cout << "id: " << id << std::endl;
                std::cout << "balance: " << balance << std::endl;

                //probably call create_account here
                try {
                    DatabaseTransactions::create_account(C, id, balance);
                } catch (const pqxx::sql_error &e) {
                    std::cout << "psql error in create_account: " << e.what() << std::endl;
                } catch (const std::exception &e) {
                    std::cout << "unknown exception in create_account: " << e.what() << std::endl;
                }

                

            } else if (std::string(element->Value()) == "symbol") {
                const char* sym = nullptr;
                element->QueryStringAttribute("sym", &sym);
                std::string symbol_name (sym);
                
                //get all children account elements
                for (tinyxml2::XMLElement* element2 = element->FirstChildElement(); element2 != nullptr; element2 = element2->NextSiblingElement()) {
                    uint32_t id = 0;
                    element2->QueryUnsignedAttribute("id", &id);

                    int num_shares = element2->IntText();

                    //probably call insert_shares here
                    try {
                        DatabaseTransactions::insert_shares(C, id, symbol_name, num_shares);
                    } catch (const pqxx::sql_error &e) {
                        std::cout << "psql error in insert_shares: " << e.what() << std::endl;
                    } catch (const std::exception &e) {
                        std::cout << "unknown exception in insert_shares: " << e.what() << std::endl;
                    }
                    
                }


            } else {
                std::cout << "received invalid element in create" << std::endl;
                break;
            }
            //const char* name = element->FirstChildElement("name")->GetText();
            //int value = 0;
            //element->FirstChildElement("value")->QueryIntText(&value);
    
        }

    } else if (std::string(root->Value()) == "transactions") {
        uint32_t id = 0;
        root->ToElement()->QueryUnsignedAttribute("id", &id);

        for (tinyxml2::XMLElement* element = root->FirstChildElement(); element != nullptr; element = element->NextSiblingElement()) {

            if (std::string(element->Value()) == "order") {
                const char* sym = nullptr;
                element->QueryStringAttribute("sym", &sym);
                std::string symbol_name (sym);

                int amount = 0;
                float limit = 0;
                element->QueryIntAttribute("amount", &amount);
                element->QueryFloatAttribute("limit", &limit);

                DatabaseTransactions::place_order(C, id, symbol_name, amount, limit);


            } else if (std::string(element->Value()) == "query") {
                int order_id = 0;
                element->QueryIntAttribute("id", &order_id);

                DatabaseTransactions::query_order(C, id, order_id);


            } else if (std::string(element->Value()) == "cancel") {
                int order_id = 0;
                element->QueryIntAttribute("id", &order_id);

                DatabaseTransactions::cancel_order(C, id, order_id);

            } else {
                std::cout << "received invalid element in transactions" << std::endl;
                break;
            }
    
        }

    } else {
        std::cout << "received invalid root element: must be create or transaction" << std::endl;
    }

    message.clear();

    return 1;

}

