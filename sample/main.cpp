#include <database.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <filesystem>

std::filesystem::path db_path = std::filesystem::absolute("sample") / "database.db";

class User : public uva::database::basic_active_record
{    
    uva_database_declare(User);
    uva_database_expose_column(name);
    uva_database_expose_column(password);
    uva_database_expose_column(age);
};

uva_database_define(User);

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
            { "age",        "INTEGER NOT NULL" },
            { "updated_at", "INTEGER NOT NULL DEFAULT (STRFTIME('%s'))" },
            { "created_at", "INTEGER NOT NULL DEFAULT (STRFTIME('%s'))" },
            { "removed",    "INTEGER NOT NULL DEFAULT 0" },
        });
    }
};

uva_define_migration(AddUsersMigration);

int main()
{
    if(std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    //Good to put under your app initialization
    uva_database_define_sqlite3(db_path);
    uva_run_migrations();

    User user;
    user.name = "Dummy";
    user.password = "Some Password";
    user.age = 21;
    user.save();

    return 0;
}