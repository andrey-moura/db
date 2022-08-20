#include <database.hpp>
#include <iostream>
#include <chrono>
#include <thread>

class User : public uva::database::basic_active_record
{    
    uva_database_declare(User)
};

uva_database_define(User);

int main()
{
   auto values = User::select("name");

    return 0;
}