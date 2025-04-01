#ifndef DATABASETRANSACTIONS_H
#define DATABASETRANSACTIONS_H
#include <string>
#include <pqxx/pqxx>

class DatabaseTransactions {
public:
    typedef std::shared_ptr<pqxx::connection> db_ptr;
    static pqxx::connection* connect();

    static int create_account(db_ptr C, uint32_t account_id, float start_balance);

    static int insert_shares(db_ptr C, uint32_t account_id, std::string& symbol, int amount);

    static int place_order(db_ptr C, uint32_t account_id, std::string& symbol, int amount, float limit);

    static std::vector<pqxx::result> query_order(db_ptr C, uint32_t account_id, int order_id);

    static std::vector<pqxx::result> cancel_order(db_ptr C, uint32_t account_id,int order_id);

};

#endif