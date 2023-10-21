#include <database.hpp>

uva::database::basic_active_record::operator var() const
{
    return values;
}