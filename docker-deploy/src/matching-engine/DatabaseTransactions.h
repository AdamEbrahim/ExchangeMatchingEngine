#ifndef DATABASETRANSACTIONS_H
#define DATABASETRANSACTIONS_H
#include <string>
#include <pqxx/pqxx>

class DatabaseTransactions {
public:
    
    static pqxx::connection* connect();

    static int create_account(uint32_t account_id, float start_balance);

    static int insert_shares(uint32_t account_id, std::string symbol, int amount);

};

#endif