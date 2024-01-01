# database

Simple cross-platform library based in the ActiveRecord from Ruby, an wrapper around SQL.

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
User user;
user.name = "Dummy";
user.password = "Some Password";
user.age = 21;
user.save();
```

### The code above executes
```sql
INSERT INTO users(name,password,age) VALUES ('Dummy','Some Password',21) RETURNING id;
```

### The same can be archivied by
```cpp
User user = User::create({
    { "name", "Dummy" },
    { "password", "Some Password" }
    { "age", 21 }
}); 
```

## Creating a new record and a table

```shell
database new-model user --migrate
```

### new-model generates the following code
#### include/models/user.hpp
```cpp
#include <core.hpp>
#include <db.hpp>

class User : public uva::db::basic_active_record
{    
    uva_database_declare(User);
};
```

#### src/models/user.cpp
```cpp
#include <user.hpp>

uva_db_define(User);
```

### The optional --migrate option generates the following migration
#### src\migrations\20230129161842_add_users_migration.cpp
```cpp
#include <core.hpp>
#include <db.hpp>

#include <user.hpp>

class AddUsersMigration : public uva::db::basic_migration
{
uva_db_declare_migration(AddUsersMigration);
protected:
    virtual void change() override 
    { 
        add_table("users",
        {
            { "id",         "INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL" },
            { "updated_at", "INTEGER NOT NULL DEFAULT (STRFTIME('%s'))" },
            { "created_at", "INTEGER NOT NULL DEFAULT (STRFTIME('%s'))" },
            { "removed",    "INTEGER NOT NULL DEFAULT 0" },
        });
    }
};

uva_db_define_migration(AddUsersMigration);
```

Columns are defined inside the map in second parameter of `add_table`. Those columns are accessed by `User::operator[](std::string)`, they'll only be available with the `User::operator.` when exposing the column inside the class:

```cpp
uva_db_expose_column(password);
```

## Supported database engines

* SQLite3

## Todo For Next (First) Release

* Before save, update 👌
* Create database tool 👌
* Move multiple_value_holder to uva::core (and rename to var) 👌
* Create database_exception
* Strongly typed var 👌
* Complete Todo List of [uva::string](https://github.com/Moonslate/core)
* Complete Todo List of [uva::string](https://github.com/Moonslate/string)
* Complete Todo List of [uva::cspec](https://github.com/Moonslate/cspec)
* Have 100% of tests coverage

### Priority (For future releases)

* 🐞 - Copy constructor of base class not works

## Contributing
Just make a PR! :)
