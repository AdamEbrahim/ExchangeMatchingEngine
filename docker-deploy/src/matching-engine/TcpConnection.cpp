#include "TcpConnection.h"
#include <iostream>
#include <sstream>
#include "tinyxml2.h"
#include <vector>
#include "DatabaseTransactions.h"
#include "CustomException.h"

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
    if (error) { //prob just EOF since connection closed, don't register another async_read_some
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
        message.clear();
        return -1;
    }

    tinyxml2::XMLNode* root = doc.FirstChildElement();
    if (root == nullptr) {
        std::cout << "Root element not found" << std::endl;
        message.clear();
        return -1;
    }

    tinyxml2::XMLDocument responseDoc;
    tinyxml2::XMLElement* respRoot = responseDoc.NewElement("results");
    responseDoc.InsertFirstChild(respRoot);

    if (std::string(root->Value()) == "create") {
        for (tinyxml2::XMLElement* element = root->FirstChildElement(); element != nullptr; element = element->NextSiblingElement()) {

            if (std::string(element->Value()) == "account") {
                uint32_t id = 0;
                float balance = 0;
                element->QueryUnsignedAttribute("id", &id);
                element->QueryFloatAttribute("balance", &balance);

                std::cout << "id: " << id << std::endl;
                std::cout << "balance: " << balance << std::endl;

                std::string error_message; //used for more user-freindly error messages
                //probably call create_account here
                try {
                    DatabaseTransactions::create_account(C, id, balance);

                } catch (const pqxx::sql_error &e) {
                    std::cout << "psql error in create_account: " << e.what() << std::endl;
                    error_message = e.what();

                } catch (const std::exception &e) {
                    std::cout << "unknown exception in create_account: " << e.what() << std::endl;
                    error_message = e.what();
                }

                tinyxml2::XMLElement* child;
                if (error_message.empty()) {
                    child = responseDoc.NewElement("created");
                } else {
                    child = responseDoc.NewElement("error");

                    if (error_message.find("duplicate key value violates unique constraint") != std::string::npos) {
                        error_message = "Account already exists.";
                    } else if (error_message.find("violates check constraint") != std::string::npos) {
                        error_message = "Balance cannot be negative.";
                    } else {
                        error_message = "Unexpected error."; //general exception handling
                    }

                    child->SetText(error_message.c_str());
                }
                
                child->SetAttribute("id", id);
                respRoot->InsertEndChild(child);
                

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
                    std::string error_message;
                    try {
                        DatabaseTransactions::insert_shares(C, id, symbol_name, num_shares);
                        
                    } catch (const pqxx::sql_error &e) {
                        std::cout << "psql error in insert_shares: " << e.what() << std::endl;
                        error_message = e.what();
                    } catch (const std::exception &e) {
                        std::cout << "unknown exception in insert_shares: " << e.what() << std::endl;
                        error_message = e.what();
                    }

                    tinyxml2::XMLElement* child;
                    if (error_message.empty()) {
                        child = responseDoc.NewElement("created");
                    } else {
                        child = responseDoc.NewElement("error");

                        if (error_message.find("violates foreign key constraint") != std::string::npos) {
                            error_message = "Account does not exist.";
                        } else if (error_message.find("violates check constraint") != std::string::npos) {
                            error_message = "Number of shares cannot be negative.";
                        } else {
                            error_message = "Unexpected error."; //general exception handling
                        }

                        child->SetText(error_message.c_str());
                    }

                    child->SetAttribute("sym", sym);
                    child->SetAttribute("id", id);
                    respRoot->InsertEndChild(child);
                    
                }


            } else {
                std::cout << "received invalid element in create" << std::endl;
            }
    
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

                std::string error_message;
                int order_id = 0;
                try {
                    order_id = DatabaseTransactions::place_order(C, id, symbol_name, amount, limit);
                    
                } catch (const CustomException& e) {
                    std::cout << "psql error in place_order: " << e.what() << std::endl;
                    error_message = e.what();
                } catch (const std::exception &e) {
                    std::cout << "unknown exception in place_order: " << e.what() << std::endl;
                    error_message = "Unexpected error."; //general exception handling
                }

                tinyxml2::XMLElement* child;
                if (error_message.empty()) {
                    child = responseDoc.NewElement("opened");

                    child->SetAttribute("sym", sym);
                    child->SetAttribute("amount", amount);
                    child->SetAttribute("limit", limit);
                    child->SetAttribute("id", order_id);
                } else {
                    child = responseDoc.NewElement("error");

                    child->SetAttribute("sym", sym);
                    child->SetAttribute("amount", amount);
                    child->SetAttribute("limit", limit);

                    child->SetText(error_message.c_str());
                }

                respRoot->InsertEndChild(child);


            } else if (std::string(element->Value()) == "query") {
                int order_id = 0;
                element->QueryIntAttribute("id", &order_id);

                std::string error_message;
                std::vector<pqxx::result> orderRes;
                try {
                    orderRes = DatabaseTransactions::query_order(C, id, order_id); //vector with 2 pqxx::result elements
                    
                } catch (const CustomException& e) {
                    std::cout << "psql error in query_order: " << e.what() << std::endl;
                    error_message = e.what();
                } catch (const std::exception &e) {
                    std::cout << "unknown exception in query_order: " << e.what() << std::endl;
                    error_message = "Unexpected error."; //general exception handling
                }

                tinyxml2::XMLElement* child;
                if (error_message.empty()) {
                    child = responseDoc.NewElement("status");

                } else {
                    child = responseDoc.NewElement("error");

                    child->SetAttribute("id", order_id);
                    child->SetText(error_message.c_str());
                    respRoot->InsertEndChild(child);
                    continue;
                }

                child->SetAttribute("id", order_id);
                respRoot->InsertEndChild(child);

                //set open, canceled, and executed elements
                pqxx::result orderEntry = orderRes[0];
                pqxx::result tradesEntry = orderRes[1];

                int originalShares = orderEntry[0]["original_shares"].as<int>();
                int openShares = orderEntry[0]["open_shares"].as<int>();
                float limitPrice = orderEntry[0]["limit_price"].as<float>();
                std::string timestamp2 = orderEntry[0]["timestamp"].as<std::string>();

                tinyxml2::XMLElement* child4 = responseDoc.NewElement("open");
                child4->SetAttribute("shares", openShares);
                child->InsertEndChild(child4);

                int totalExecuted = 0;
                for (const auto& row : tradesEntry) { //can have multiple exectued trades
                    int tradeId = row["trade_id"].as<int>();
                    int tradedShares = row["traded_shares"].as<int>();
                    float price = row["price"].as<float>();
                    std::string timestamp = row["timestamp"].as<std::string>();

                    tinyxml2::XMLElement* child2 = responseDoc.NewElement("executed");
                    child2->SetAttribute("shares", tradedShares);
                    child2->SetAttribute("price", price);
                    child2->SetAttribute("time", timestamp.c_str());

                    child->InsertEndChild(child2);


                    totalExecuted += tradedShares;
                
                }

                int canceled = originalShares - openShares - totalExecuted;
                if (canceled != 0) {
                    tinyxml2::XMLElement* child3 = responseDoc.NewElement("canceled");
                    child3->SetAttribute("shares", canceled);
                    child3->SetAttribute("time", timestamp2.c_str()); //timestamp in orders will be updated if cancel an order
                    child->InsertEndChild(child3);
                }


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
        message.clear();
        return -1;
    }

    //create string from xmlResponse
    tinyxml2::XMLPrinter printer;
    responseDoc.Print(&printer);
    std::string responseString = printer.CStr();
    std::cout << "response xml: " << std::endl;
    std::cout << responseString << std::endl;

    message.clear();

    return 1;

}

