#include <database.hpp>
#include <iostream>
#include <chrono>
#include <thread>



class User : public uva::database::basic_active_record
{    
    uva_database_declare(User)
};

uva_database_define_sqlite3(User, uva_database_params(
{
    { "name", "TEXT" },
    { "email", "TEXT" },
}), std::filesystem::absolute("database") / "database.db");

int main()
{
   auto values = User::select("name");

    return 0;
}