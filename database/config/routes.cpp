#include <iostream>
#include <application.hpp>

using namespace uva;
using namespace routing;
using namespace console;

#include <database_controller.hpp>

DECLARE_CONSOLE_APPLICATION(
    //Declare your routes above. As good practice, keep then ordered by controler.
    //You can have C++ code here, perfect for init other libraries.

    ROUTE("model", database_controller::new_model);
    ROUTE("migration", database_controller::new_migration);
    ROUTE("init", database_controller::init);
)
