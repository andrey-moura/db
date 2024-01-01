#include <db.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <filesystem>

std::filesystem::path db_path = std::filesystem::absolute("sample") / "database.db";

class User : public uva::db::basic_active_record
{    
    uva_database_declare(User);
    uva_db_expose_column(name);
    uva_db_expose_column(password);
    uva_db_expose_column(age);
};

uva_db_define(User);

class AddUsersMigration : public uva::db::basic_migration
{
uva_db_declare_migration(AddUsersMigration);
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

uva_db_define_migration(AddUsersMigration);

int main()
{
    if(std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    //Good to put under your app initialization
    uva_db_define_sqlite3(db_path);
    uva_db_run_migrations();

    User user;
    user.name = "Dummy";
    user.password = "Some Password";
    user.age = 21;
    user.save();

    return 0;
}