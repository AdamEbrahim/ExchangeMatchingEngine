#include "DatabaseTransactions.h"
#include <pqxx/pqxx>
#include <exception>
#include <iostream>
#include "CustomException.h"
#include <algorithm>
#include <thread>
#include <chrono>

extern thread_local std::shared_ptr<pqxx::connection> thread_conn; //make thread local db connection visible

void DatabaseTransactions::setup() {
    //RESET AND SETUP TABLES HERE
    //dont think i need to setup a transaction as only 1 thread here
    try {
        pqxx::work W(*thread_conn);
    
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
        throw; //propagate exception
    }
}

int DatabaseTransactions::create_account(uint32_t account_id, float start_balance) {
    pqxx::work W(*thread_conn);
    
    W.exec_params("INSERT INTO Accounts (account_id, balance) VALUES ($1, $2);",
                    account_id, start_balance);
    
    W.commit();
    return 1; //success if didn't throw (should get caught in caller)
}

//only support inserting int number of shares, might need to extend to partial shares?
int DatabaseTransactions::insert_shares(uint32_t account_id, std::string& symbol, int amount) {
    pqxx::work W(*thread_conn);

    pqxx::result holdingRes = W.exec_params(
        "SELECT amount FROM Holdings WHERE account_id = $1 AND symbol = $2 FOR UPDATE;", //make sure to acquire row lock
        account_id, symbol
    );

    if (holdingRes.empty()) {
        W.exec_params("INSERT INTO Holdings (account_id, symbol, amount) "
                        "VALUES ($1, $2, $3);",
                        account_id, symbol, amount);
    } else {
        W.exec_params("UPDATE Holdings SET amount = amount + $1 "
                        "WHERE account_id = $2 AND symbol = $3;",
                        amount, account_id, symbol);
    }

    W.commit();
    return 1;
}

//ensures order is opened and balance/current holdings are updated atomically using row level lock
int DatabaseTransactions::place_order(uint32_t account_id, std::string& symbol, int amount, float limit) {
    pqxx::work W(*thread_conn);

    pqxx::result res;
    if (amount >= 0) { //buy; just handle orders of 0 as well

        //make sure balance is enough
        res = W.exec_params(
            "SELECT balance FROM Accounts WHERE account_id = $1 FOR UPDATE;", //lock
            account_id
        );

        if (res.empty()) {
            throw CustomException("Account does not exist.");
        }

        float curr_balance = res[0]["balance"].as<float>();

        if (curr_balance < limit * amount) {
            throw CustomException("Insufficient balance.");
        }

        //can release lock and update as we have guaranteed have resources
        W.exec_params(
            "UPDATE Accounts SET balance = balance - $1 WHERE account_id = $2;",
            limit * amount, account_id
        );

    } else { //sell
        //check if account exists; not going to be updating account table so no lock
        res = W.exec_params(
            "SELECT balance FROM Accounts WHERE account_id = $1;",
            account_id
        );

        if (res.empty()) {
            throw CustomException("Account does not exist.");
        }

        //make sure current shares is enough to sell
        res = W.exec_params(
            "SELECT amount FROM Holdings WHERE account_id = $1 AND symbol = $2 FOR UPDATE;", //lock
            account_id, symbol
        );

        if (res.empty()) {
            throw CustomException("Account does not own shares of this symbol.");
        }

        int curr_amount = res[0]["amount"].as<int>();

        //remember amount is negative since sell
        if (curr_amount < -1 * amount) {
            throw CustomException("Insufficient currently owned shares of this symbol.");
        }

        //can release lock and update as we have guaranteed have resources
        W.exec_params(
            "UPDATE Holdings SET amount = amount + $1 WHERE account_id = $2 AND symbol = $3;",
            amount, account_id, symbol
        );

    }

    //create new order
    res = W.exec_params("INSERT INTO Orders (account_id, symbol, original_shares, open_shares, limit_price) "
        "VALUES ($1, $2, $3, $4, $5) RETURNING order_id;",
        account_id, symbol, amount, amount, limit);
    int order_id = res[0][0].as<int>(); //store newly created order id so we can return it


    //do matching; order by order_id to create consistent order of row level locks in FOR UPDATE to prevent deadlock
    res = W.exec_params(
        "SELECT * FROM Orders "
        "WHERE symbol = $1 AND open_shares > 0 "
        "ORDER BY order_id ASC FOR UPDATE;",  //Consistent order by order_id
        symbol);

    //buy and sell lists
    std::vector<pqxx::row> buy_orders, sell_orders;
    for (const auto& row : res) {
        int open_shares = row["open_shares"].as<int>();

        if (open_shares > 0) {
            buy_orders.push_back(row);
        } else {
            sell_orders.push_back(row);
        }
    }

    //sort buy orders: highest limit price first, break ties with earliest timestamp
    std::sort(buy_orders.begin(), buy_orders.end(), [](const pqxx::row& a, const pqxx::row& b) {
        double priceA = a["limit_price"].as<double>();
        double priceB = b["limit_price"].as<double>();
        return (priceA > priceB) || (priceA == priceB && a["timestamp"].as<std::string>() < b["timestamp"].as<std::string>());
    });

    //sort sell orders: lowest limit price first, break ties with earliest timestamp
    std::sort(sell_orders.begin(), sell_orders.end(), [](const pqxx::row& a, const pqxx::row& b) {
        double priceA = a["limit_price"].as<double>();
        double priceB = b["limit_price"].as<double>();
        return (priceA < priceB) || (priceA == priceB && a["timestamp"].as<std::string>() < b["timestamp"].as<std::string>());
    });

    int buy_index = 0, sell_index = 0;
    while (buy_index < buy_orders.size() && sell_index < sell_orders.size()) {
        pqxx::row& buy = buy_orders[buy_index];
        pqxx::row& sell = sell_orders[sell_index];

        int buy_id = buy["order_id"].as<int>();
        int sell_id = sell["order_id"].as<int>();
        int buyer_account = buy["account_id"].as<int>();
        int seller_account = sell["account_id"].as<int>();
        int buy_shares = buy["open_shares"].as<int>();
        int sell_shares = -sell["open_shares"].as<int>();
        double buy_price = buy["limit_price"].as<double>();
        double sell_price = sell["limit_price"].as<double>();
        std::string buy_time = buy["timestamp"].as<std::string>();
        std::string sell_time = sell["timestamp"].as<std::string>();

        if (buy_price < sell_price) {
            break; //no more possible matches
        }

        double exec_price = (buy_time < sell_time) ? buy_price : sell_price;

        int trade_shares = std::min(buy_shares, sell_shares);

        //update buyer; no concurrency issues as if any other transaction holds the lock this will pause on update
        W.exec_params(
            "INSERT INTO Holdings (account_id, symbol, amount) VALUES ($1, $2, $3) "
            "ON CONFLICT (account_id, symbol) DO UPDATE SET amount = Holdings.amount + EXCLUDED.amount;",
            buyer_account, symbol, trade_shares
        );

        //update seller; no concurrency issues as if any other transaction holds the lock this will pause
        W.exec_params(
            "UPDATE Accounts SET balance = balance + $1 WHERE account_id = $2;",
            trade_shares * exec_price, seller_account
        );

        //update orders
        W.exec_params("UPDATE Orders SET open_shares = open_shares - $1 WHERE order_id = $2;", trade_shares, buy_id);
        W.exec_params("UPDATE Orders SET open_shares = open_shares + $1 WHERE order_id = $2;", trade_shares, sell_id);

        //insert trade
        W.exec_params(
            "INSERT INTO Trades (buy_order_id, sell_order_id, symbol, shares, price) "
            "VALUES ($1, $2, $3, $4, $5);",
            buy_id, sell_id, symbol, trade_shares, exec_price
        );

        //move pointer
        if (trade_shares == buy_shares) buy_index++;
        if (trade_shares == sell_shares) sell_index++;
    }

    W.commit(); //lock released on all rows, another thread trying to run matching can continue

    return order_id;
}

std::vector<pqxx::result> DatabaseTransactions::query_order(uint32_t account_id, int order_id) {
    pqxx::work W(*thread_conn);

    //check if account exists; not going to be updating account table so no lock
    pqxx::result orderRes = W.exec_params(
        "SELECT balance FROM Accounts WHERE account_id = $1;",
        account_id
    );

    if (orderRes.empty()) {
        throw CustomException("Account does not exist.");
    }

    //check if order exists and belongs to the account, lock order row
    orderRes = W.exec_params(
        "SELECT original_shares, open_shares, limit_price, timestamp FROM Orders "
        "WHERE order_id = $1 AND account_id = $2 FOR UPDATE;",
        order_id, account_id
    );

    if (orderRes.empty()) {
        throw CustomException("Transaction with given id does not exist.");
    }

    std::vector<pqxx::result> res;
    res.push_back(orderRes);

    //since order row locked, no new executed trades can be inserted for it, no locking needed
    orderRes = W.exec_params(
        "SELECT trade_id, traded_shares, price, timestamp "
        "FROM Trades WHERE buy_order_id = $1 OR sell_order_id = $1",
        order_id
    );
    res.push_back(orderRes);

    W.commit();

    return res;

}

std::vector<pqxx::result> DatabaseTransactions::cancel_order(uint32_t account_id,int order_id) {
    pqxx::work W(*thread_conn);

    //check if account exists; not going to be updating account table so no lock
    pqxx::result orderRes = W.exec_params(
        "SELECT balance FROM Accounts WHERE account_id = $1;",
        account_id
    );

    if (orderRes.empty()) {
        throw CustomException("Account does not exist.");
    }

    //lock order row
    orderRes = W.exec_params(
        "SELECT * FROM Orders "
        "WHERE order_id = $1 AND account_id = $2 FOR UPDATE",
        order_id, account_id
    );

    if (orderRes.empty()) {
        throw CustomException("Transaction with given id does not exist.");
    }

    std::vector<pqxx::result> res;

    int openShares = orderRes[0]["open_shares"].as<int>();
    float limitPrice = orderRes[0]["limit_price"].as<float>();
    std::string symbol = orderRes[0]["symbol"].as<std::string>();

    if (openShares == 0) {
        throw CustomException("Transaction already fully executed or canceled.");
    }

    if (openShares > 0) { //buy
        //refund balance, will wait for lock
        W.exec_params(
            "UPDATE Accounts SET balance = balance + $1 WHERE account_id = $2;",
            openShares * limitPrice, account_id
        );


    } else { //sell
        //refund shares, will wait for lock
        W.exec_params(
            "INSERT INTO Holdings (account_id, symbol, amount) VALUES ($1, $2, $3) "
            "ON CONFLICT (account_id, symbol) DO UPDATE SET amount = Holdings.amount + EXCLUDED.amount;",
            account_id, symbol, openShares * -1
        );

    }

    //since order row locked, no new executed trades can be inserted for it, no locking needed
    orderRes = W.exec_params(
        "SELECT trade_id, traded_shares, price, timestamp "
        "FROM Trades WHERE buy_order_id = $1 OR sell_order_id = $1;",
        order_id
    );


    //update order to reflect as cancelled, update timestamp
    W.exec_params(
        "UPDATE Orders SET open_shares = 0, timestamp = now() WHERE order_id = $1;",
        order_id
    );

    pqxx::result orderRes2 = W.exec_params(
        "SELECT original_shares, open_shares, limit_price, timestamp FROM Orders "
        "WHERE order_id = $1;",
        order_id
    );

    res.push_back(orderRes2);
    res.push_back(orderRes);

    W.commit();

    return res;
}
