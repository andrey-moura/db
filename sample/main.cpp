#include <database.hpp>
#include <iostream>
#include <chrono>
#include <thread>

#include "include/uva/console/console.hpp"

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
    //uva::database::active_record_relation users = User::where();

    std::cout << "Test" << std::endl;
    //std::cout << uva::console::color(uva::console::color_code::yellow) << "This is yellow, color yellow." << std::endl;

      HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  // you can loop k higher to see more color choices
  for(int k = 1; k < 255; k++)
  {
    // pick the colorattribute k you want
    SetConsoleTextAttribute(hConsole, k);
    std::cout << k << " I want to be nice today!" << std::endl;
  }

  std::this_thread::sleep_for(std::chrono::duration<float>(2));

    // User user = User::create({
    //     {"email", "someuser@email.com"},
    //     {"name", "some name"}
    // });
    // //saved in database and cached in memory

    // std::string name = user["name"];
    // std::cout << name << "\n";

    // std::cout << (std::string)user["email"] << "\n";

    return 0;
}