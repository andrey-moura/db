# database

Pretty simple cross-platform library based in the ActiveRecord from Ruby, for queryging and writing, an wrapper around SQL.

## Building 

### Linux

```shell
git clone https://github.com/Moonslate/database.git
mkdir build
cd build
cmake ..
make
```

## Basic Usage

```cpp
#include <database.hpp>
#include <iostream>

class User : public uva::database::basic_active_record
{    
    uva_database_declare(User)
};

uva_database_define(User);

uva_database_define_sqlite3("database.db");

int main()
{
    //saved in database and cached in memory
    User user = User::create({
        {"email", "someuser@email.com"},
        {"name", "some name"}
    });    

    std::string name = user["name"];
    std::cout << name << "\n";

    std::cout << (std::string)user["email"] << "\n";
    return 0;
}
```

### Creating a new table

```cpp

#include <database.hpp>

class AddUsersMigration : public uva::database::basic_migration
{
uva_declare_migration(AddUsersMigration);
protected:
    virtual void change() override
    {
        add_table("users",
        {
            { "name",        "TEXT NOT NULL" },
            { "permissions", "TEXT" },
            { "password",    "TEXT NOT NULL" },
        });
    }
};

uva_define_migration(AddUsersMigration);

```

## Contributing
Just make a PR! :)
