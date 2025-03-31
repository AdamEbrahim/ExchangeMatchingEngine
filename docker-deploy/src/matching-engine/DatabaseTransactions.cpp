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
    
        //drop existing tables to reset
        W.exec("DROP TABLE IF EXISTS Trades CASCADE;");
        W.exec("DROP TABLE IF EXISTS Orders CASCADE;");
        W.exec("DROP TABLE IF EXISTS Holdings CASCADE;");
        W.exec("DROP TABLE IF EXISTS Accounts CASCADE;");
    
        //accounts table
        W.exec("CREATE TABLE Accounts ("
               "account_id BIGINT PRIMARY KEY,"
               "balance FLOAT NOT NULL CHECK (balance >= 0));");
    
        //orders table
        W.exec("CREATE TABLE Orders ("
               "order_id SERIAL PRIMARY KEY,"
               "account_id BIGINT,"
               "symbol VARCHAR(20) NOT NULL,"
               "original_shares INTEGER NOT NULL," //amount of shares initially requested (buy = positive, sell = negative)
               "open_shares INTEGER NOT NULL,"  //amount of shares open (buy = positive, sell = negative)
               "limit_price FLOAT NOT NULL,"  //user given limit price
               "timestamp TIMESTAMP DEFAULT now()," //when order arrived
               "FOREIGN KEY (account_id) REFERENCES ACCOUNTS(account_id) ON DELETE CASCADE);");
    
        //executed trades table
        W.exec("CREATE TABLE Trades ("
               "trade_id SERIAL PRIMARY KEY,"
               "buy_order_id INTEGER,"
               "sell_order_id INTEGER,"
               "symbol VARCHAR(20) NOT NULL,"
               "traded_shares INTEGER NOT NULL," //how many shares were traded in this trade (could be partial execution)
               "price FLOAT NOT NULL," //price the trade was executed at
               "timestamp TIMESTAMP DEFAULT now(),"
               "FOREIGN KEY (buy_order_id) REFERENCES ORDERS(order_id) ON DELETE SET NULL," //setting NULL on delete of the connected order_id, might need to CASCADE
               "FOREIGN KEY (sell_order_id) REFERENCES ORDERS(order_id) ON DELETE SET NULL);");
    
        //holdings (symbol ownership) table
        W.exec("CREATE TABLE Holdings ("
               "id SERIAL PRIMARY KEY,"
               "account_id BIGINT,"
               "symbol VARCHAR(20) NOT NULL,"
               "amount INTEGER NOT NULL CHECK (amount >= 0),"
               "FOREIGN KEY (account_id) REFERENCES ACCOUNTS(account_id) ON DELETE CASCADE,"
               "UNIQUE (account_id, symbol));");
    
        W.commit();
        std::cout << "successfully setup db tables" << std::endl;
    } catch (const std::exception &e) {
        std::cout << "Error setup db tables: " << e.what() << std::endl;
        return nullptr;
    }
    

    return C;
}

int DatabaseTransactions::create_account(db_ptr C, uint32_t account_id, float start_balance) {
    pqxx::work W(*C);
    
    W.exec_params("INSERT INTO Accounts (account_id, balance) VALUES ($1, $2);",
                    account_id, start_balance);
    
    W.commit();
    return 1; //success if didn't throw (should get caught in caller)
}

//only support inserting int number of shares, might need to extend to partial shares?
int DatabaseTransactions::insert_shares(db_ptr C, uint32_t account_id, std::string& symbol, int amount) {
    pqxx::work W(*C);
    
    //insert new holding or update existing one
    W.exec_params("INSERT INTO Holdings (account_id, symbol, amount) "
                "VALUES ($1, $2, $3) "
                "ON CONFLICT (account_id, symbol) "
                "DO UPDATE SET amount = Holdings.amount + EXCLUDED.amount;",
                account_id, symbol, amount);

    W.commit();
    return 1;
}


int DatabaseTransactions::place_order(db_ptr C, uint32_t account_id, std::string& symbol, int amount, float limit) {
    return 1;
}

int DatabaseTransactions::query_order(db_ptr C, uint32_t account_id, int order_id) {
    return 1;
}

int DatabaseTransactions::cancel_order(db_ptr C, uint32_t account_id,int order_id) {
    return 1;
}

//function to actually match orders after placing a new order
//should probably be part of the same transaction as place_order?
int DatabaseTransactions::perform_match(db_ptr C, std::string& symbol) {
    return 1;
}