#include <application.hpp>

using namespace uva;
using namespace routing;
using namespace console;

class database_controller : public basic_controller
{
public:
    void init();
    void new_migration();
    void new_model();
};
