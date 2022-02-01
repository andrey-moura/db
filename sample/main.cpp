#include <database.hpp>
#include <iostream>

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
    User user = User::create({
        {"email", "someuser@email.com"},
        {"name", "some name"}
    });
    //saved in database and cached in memory

    std::string name = user["name"];
    std::cout << name << "\n";

    std::cout << (std::string)user["email"] << "\n";
    return 0;
}