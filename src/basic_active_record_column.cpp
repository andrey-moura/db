#include <db.hpp>

uva::db::basic_active_record_column::basic_active_record_column(const std::string& __key, uva::db::basic_active_record* __record)
    : key(__key), active_record(__record)
{

}

uva::db::basic_active_record_column::~basic_active_record_column()
{

}

const var *uva::db::basic_active_record_column::operator->() const
{
    return &active_record->at(key);
}

var* uva::db::basic_active_record_column::operator->()
{
    return &active_record->at(key);
}

var& uva::db::basic_active_record_column::operator*() const
{
    return active_record->at(key);
}

var& uva::db::basic_active_record_column::operator*()
{
    return active_record->at(key);
}

uva::db::basic_active_record_column::operator var() const
{
    return active_record->at(key);
}