#include <database.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <filesystem>

class User : public uva::database::basic_active_record
{    
    uva_database_declare(User);
    uva_database_expose_column(name);
    uva_database_expose_column(password);
};

uva_database_define(User);
uva_database_define_sqlite3(std::filesystem::absolute("db") / "database.db");

class AddUsersMigration : public uva::database::basic_migration
{
uva_declare_migration(AddUsersMigration);
protected:
    virtual void change() override 
    { 
        add_table("users",
        {
            { "id",         "INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL" },
            { "name",       "TEXT NOT NULL" },
            { "password",   "TEXT NOT NULL" },
            { "updated_at", "INTEGER NOT NULL DEFAULT (STRFTIME('%s'))" },
            { "created_at", "INTEGER NOT NULL DEFAULT (STRFTIME('%s'))" },
            { "removed",    "INTEGER NOT NULL DEFAULT 0" },
        });
    }
};

uva_define_migration(AddUsersMigration);

int main()
{
   User user;
   user.name = "Andrey";
   user.password = "Wci010203";
   user.save();

    return 0;
}