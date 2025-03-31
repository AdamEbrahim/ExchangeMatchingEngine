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

    //RESET AND SETUP TABLES HERE
    //dont think i need to setup a transaction as only 1 thread here
    try {
        pqxx::work W(*C);
    
        // Drop existing tables to reset
        W.exec("DROP TABLE IF EXISTS Trades CASCADE;");
        W.exec("DROP TABLE IF EXISTS Orders CASCADE;");
        W.exec("DROP TABLE IF EXISTS Holdings CASCADE;");
        W.exec("DROP TABLE IF EXISTS Accounts CASCADE;");
    
        // Create Accounts table
        W.exec("CREATE TABLE Accounts ("
               "account_id SERIAL PRIMARY KEY,"
               "balance FLOAT NOT NULL CHECK (balance >= 0));");
    
        // Create Orders table (supports partial execution)
        W.exec("CREATE TABLE Orders ("
               "order_id SERIAL PRIMARY KEY,"
               "account_id INTEGER REFERENCES Accounts(account_id) ON DELETE CASCADE,"
               "symbol VARCHAR(10) NOT NULL,"
               "amount INTEGER NOT NULL,"  // Total order size (buy = positive, sell = negative)
               "amount_remaining INTEGER NOT NULL,"  // Unfilled portion
               "price FLOAT NOT NULL,"  // Limit price
               "status VARCHAR(20) NOT NULL DEFAULT 'open' CHECK (status IN ('open', 'partially_executed', 'executed', 'canceled')),"
               "timestamp TIMESTAMP DEFAULT now());");
    
        // Create Trades table (logs executions)
        W.exec("CREATE TABLE Trades ("
               "trade_id SERIAL PRIMARY KEY,"
               "buy_order_id INTEGER REFERENCES Orders(order_id) ON DELETE SET NULL,"
               "sell_order_id INTEGER REFERENCES Orders(order_id) ON DELETE SET NULL,"
               "symbol VARCHAR(10) NOT NULL,"
               "trade_amount INTEGER NOT NULL,"  // How many shares were matched
               "price FLOAT NOT NULL,"  // Price at execution
               "timestamp TIMESTAMP DEFAULT now());");
    
        // Create Holdings table (tracks ownership)
        W.exec("CREATE TABLE Holdings ("
               "id SERIAL PRIMARY KEY,"
               "account_id INTEGER REFERENCES Accounts(account_id) ON DELETE CASCADE,"
               "symbol VARCHAR(10) NOT NULL,"
               "amount INTEGER NOT NULL CHECK (amount >= 0),"
               "UNIQUE (account_id, symbol));");
    
        W.commit();
        std::cout << "Database tables reset and initialized successfully." << std::endl;
    } catch (const std::exception &e) {
        std::cout << "Error settup up db tables: " << e.what() << std::endl;
        return nullptr;
    }
    

    return C;
}

int DatabaseTransactions::create_account(uint32_t account_id, float start_balance) {
    return 1;


}

//only support inserting int number of shares, might need to extend to partial shares?
int DatabaseTransactions::insert_shares(uint32_t account_id, std::string symbol, int amount) {
    return 1;
}