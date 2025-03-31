#include "DatabaseTransactions.h"
#include <pqxx/pqxx>
#include <exception>
#include <iostream>

pqxx::connection* DatabaseTransactions::connect() {
    pqxx::connection* C;
    try {
        C = new pqxx::connection("dbname=postgres user=postgres password=postgres host=db port=5432");
        if (!C->is_open()) {
            std::cout << "Error: cant open db" << std::endl;
            return nullptr;
        }

    } catch (std::exception& e) {
        std::cout << e.what() << std::endl;
        return nullptr;
    }

    std::cout << "successfully connected to db" << std::endl;

    //TODO: RESET AND SETUP TABLES HERE

    return C;
}

int DatabaseTransactions::create_account(int id, int start_balance) {
    return 1;


}

int DatabaseTransactions::insert_shares(std::string symbol, int amount) {
    return 1;
}