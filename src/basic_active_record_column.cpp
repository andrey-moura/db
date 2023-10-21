#include <database.hpp>

uva::database::basic_active_record_column::basic_active_record_column(const std::string& __key, uva::database::basic_active_record* __record)
    : key(__key), active_record(__record)
{

}

uva::database::basic_active_record_column::~basic_active_record_column()
{

}

const var *uva::database::basic_active_record_column::operator->() const
{
    return &active_record->at(key);
}

var* uva::database::basic_active_record_column::operator->()
{
    return &active_record->at(key);
}

var& uva::database::basic_active_record_column::operator*() const
{
    return active_record->at(key);
}

var& uva::database::basic_active_record_column::operator*()
{
    return active_record->at(key);
}

uva::database::basic_active_record_column::operator var() const
{
    return active_record->at(key);
}