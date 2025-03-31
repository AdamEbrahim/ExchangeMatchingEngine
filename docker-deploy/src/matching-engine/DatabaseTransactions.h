#ifndef DATABASETRANSACTIONS_H
#define DATABASETRANSACTIONS_H
#include <string>
#include <pqxx/pqxx>

class DatabaseTransactions {
public:
    
    static pqxx::connection* connect();

    static int create_account(int id, int start_balance);

    static int insert_shares(std::string symbol, int amount);

};

#endif