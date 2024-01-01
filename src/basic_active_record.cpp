#include <database.hpp>

uva::db::basic_active_record::operator var() const
{
    return values;
}