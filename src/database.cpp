#include <database.hpp>

//BASIC CONNECTION

std::map<std::string, uva::database::basic_connection*> uva::database::basic_connection::s_connections;

//SQLITE3 CONNECTION

uva::database::sqlite3_connection::sqlite3_connection(const std::filesystem::path& database_path)
    : m_database_path(database_path)
{
    open();
}

uva::database::sqlite3_connection::~sqlite3_connection()
{
    sqlite3_close(m_database);
}

bool uva::database::sqlite3_connection::open()
{
    std::filesystem::path folder = m_database_path.parent_path();
    if (!std::filesystem::exists(folder)) {
        if (!std::filesystem::create_directories(folder)) {
            throw std::runtime_error("unknow error while creating databse directories.");
        }
    }
    int error = sqlite3_open(m_database_path.string().c_str(), &m_database);    
    if (error) {        
        throw std::runtime_error("unknow error while opening database.");
    }
    return m_database;
}

bool uva::database::sqlite3_connection::open(const std::filesystem::path& path)
{
    m_database_path = path;
    int error = sqlite3_open(m_database_path.string().c_str(), &m_database);
    if (error) {
        throw std::runtime_error("unknow error while opening databse.");
    }
    return m_database;
}

bool uva::database::sqlite3_connection::create_table(const uva::database::table* table) const
{
    /* Create SQL statement */
    std::string sql = "CREATE TABLE IF NOT EXISTS " + table->m_name + "("  \
    "ID INT PRIMARY KEY NOT NULL ";

    for(const auto& row : table->m_rows)
    {
        sql += ", " + row.first + " " + row.second;
    }

    sql += ");";

    /* Execute SQL statement */

    char* error_msg = nullptr;
    int error = sqlite3_exec(m_database, sql.c_str(),
        [](void *NotUsed, int columnCount, char **columnValue, char **value) {
        return 0;
    }, 0, &error_msg);

    if (error) {
        std::string error_report = error_msg;
        sqlite3_free(error_msg);
        throw std::runtime_error(error_report);
    }

    return true;
}

void uva::database::sqlite3_connection::read_table(table* table)
{
        /* Create SQL statement */
    std::string sql = "SELECT * FROM " + table->m_name;

    /* Execute SQL statement */

    char* error_msg = nullptr;
    int error = sqlite3_exec(m_database, sql.c_str(),
        [](void *data, int columnCount, char** values, char** colNames )
    {
        uva::database::table* table = (uva::database::table*)data;
        std::map<std::string, std::string> relations;

        for(size_t i = 1; i < columnCount; ++i) {
            relations.insert({ colNames[i], values[i] });
        }         

        table->m_relations.insert({ std::stoi(values[0]), relations });

        return 0;
    }, table, &error_msg);

    if (error) {
        std::string error_report = error_msg;
        sqlite3_free(error_msg);
        throw std::runtime_error(error_report);
    }    
}

bool uva::database::sqlite3_connection::insert(table* table, size_t id, const std::map<std::string, std::string>& relations) {
    char* error_msg = nullptr;
    std::string sql = "INSERT INTO " + table->m_name + " VALUES(" + std::to_string(id);
    
    for(const auto& row : table->m_rows) {
        sql += ", \"";
        auto it = relations.find(row.first);
        if(it != relations.end()) {
            if (row.second == "TEXT") {
                sql.reserve(sql.size() + it->second.size());
                for (const char& c : it->second) {
                    if (c == '\"') {
                        sql.push_back('\"');
                    }
                    sql.push_back(c);
                }                
            }
        }
        sql += "\"";
    }
    
    sql += ");";
    int error = sqlite3_exec(m_database, sql.c_str(), nullptr, nullptr, &error_msg);
            
    if (error) {
        std::string error_report = error_msg;
        sqlite3_free(error_msg);
        throw std::runtime_error(error_report);
    }
    return true;
}

bool uva::database::sqlite3_connection::is_open() const
{
    return m_database;
}

void uva::database::sqlite3_connection::update(size_t id, const std::string& key, const std::string& value, uva::database::table* table) {
    char* error_msg = nullptr;
    std::string sql = "UPDATE " + table->m_name + " SET " + key + " = '" + value + "' where id = " + std::to_string(id);    
    int error = sqlite3_exec(m_database, sql.c_str(), nullptr, nullptr, &error_msg);    
    if (error) {
        std::string error_report = error_msg;
        sqlite3_free(error_msg);
        throw std::runtime_error(error_report);
    }
}

void uva::database::sqlite3_connection::destroy(size_t id, uva::database::table* table)
{
    char* error_msg = nullptr;
    std::string sql = "DELETE FROM " + table->m_name + " where id = " + std::to_string(id);    
    int error = sqlite3_exec(m_database, sql.c_str(), nullptr, nullptr, &error_msg);    
    if (error) {
        std::string error_report = error_msg;
        sqlite3_free(error_msg);
        throw std::runtime_error(error_report);
    }
}

uva::database::sqlite3_connection* uva::database::sqlite3_connection::connect(const std::filesystem::path& database)
{
    uva::database::basic_connection*& connection = uva::database::basic_connection::s_connections[database.string()];
    if(connection) return (uva::database::sqlite3_connection*)connection;

    connection = new uva::database::sqlite3_connection(database);    
    return (uva::database::sqlite3_connection*)connection;
}

//END SQLITE3 CONNECTION

//VALUE

uva::database::value::value(const std::string& key, std::string& value, size_t id, table* table)
    : m_key(key), m_value(value), m_recordId(id), m_table(table)
{

}

uva::database::value& uva::database::value::operator=(const std::string& other)
{
    m_table->update(m_recordId, m_key, other);
    m_value = other;
    return *this;
}

//END VALUE

//TABLE

uva::database::table::table(const std::string& name,
    const std::map<std::string, std::string>& rows,
    basic_connection* connection)

    : m_connection(connection), m_rows(rows), m_name(name)
{
    get_tables().insert({name, this});
    m_connection->create_table(this);
    m_connection->read_table(this);
}

std::map<std::string, uva::database::table*>& uva::database::table::get_tables()
{
    static std::map<std::string, table*> s_tables;
    return s_tables;
}

size_t uva::database::table::create(const std::map<std::string, std::string>& relations)
{
    size_t id = 0;
    auto last = m_relations.rbegin();
    if (last != m_relations.rend()) {
        id = last->first + 1;
    }

    if (m_connection->insert(this, id, relations)) {
        m_relations.insert({ id, relations });
        return id;
    }

    return std::string::npos;
}

size_t uva::database::table::create() {
    std::map<std::string, std::string> empty_relations;
    for (const auto& row : m_rows) {
        empty_relations.insert({ row.first, { "" } });
    }
    return create(empty_relations);
}

void uva::database::table::destroy(size_t id) {
    m_connection->destroy(id, this);
    m_relations.erase(id);
}

bool uva::database::table::relation_exists(size_t id) const {
    return m_relations.find(id) != m_relations.end();
}

size_t uva::database::table::find(size_t id) const {
    if (id == std::string::npos) return std::string::npos;
    auto it = m_relations.find(id);
    if(it != m_relations.end()) {
        return it->first;
    }
    return std::string::npos;
}

size_t uva::database::table::find_by(const std::map<std::string, std::string>& relations)
{
    for (const auto& record : m_relations) {
        bool match = true;
        for (const auto& relation : relations) {
            if (relation.second != record.second.find(relation.first)->second) {
                match = false;
                break;
            }            
        }
        if (match) return record.first;
    }

    return std::string::npos;
}

uva::database::active_record_collection uva::database::table::where(const std::map<std::string, std::string>& relations)
{
    active_record_collection collection;
    for (const auto& record : m_relations) {
        for (const auto& relation : relations) {
            auto it = record.second.find(relation.first);
            if (it == record.second.end()) {
                throw std::out_of_range("There is no column named " + relation.first + " in the table " + m_name);
            }
            if (it->second == relation.second) {
                collection << record.first;
            }
        }
    }

    return active_record_collection();
}

void uva::database::table::update(size_t id, const std::string& key, const std::string& value) {
    m_connection->update(id, key, value, this);
}

//END TABLE

//ACTIVE RECORD

uva::database::basic_active_record::basic_active_record(size_t _id)
    : id(_id) {
    
}

uva::database::basic_active_record::basic_active_record(const basic_active_record& _record)
    : id(_record.id)
{

}

uva::database::basic_active_record::basic_active_record(value& value)
{
    if (value.present()) {
        std::string v = value;
        for (const char& c : v) {
            if (!isdigit(c)) {
                //
                return;
            }
        }

        id = std::stol(v);
    }
    else {
        //
    }
}

bool uva::database::basic_active_record::present() const {
    return get_table()->relation_exists(id);
}

void uva::database::basic_active_record::destroy() {
    get_table()->destroy(id);
    id = -1;
}

uva::database::value uva::database::basic_active_record::at(size_t index) {
    uva::database::table* table = get_table();
    auto recordIt = table->m_relations.find(id);
    if (recordIt == table->m_relations.end()) {
        throw std::out_of_range(table->m_name + " do not contains a record with id " + std::to_string(id));
    }

    if( index > recordIt->second.size()-1) {
        throw std::out_of_range(table->m_name + " do not contains a column with index " + std::to_string(index));
    }

    auto relationIt = recordIt->second.begin();
    std::advance(relationIt, index);
        
    //relationIt->se
    return uva::database::value(relationIt->first, relationIt->second, id, table);
}

uva::database::value uva::database::basic_active_record::at(const std::string& key) {
    uva::database::table* table = get_table();
    auto recordIt = table->m_relations.find(id);
    if (recordIt == table->m_relations.end()) {
        throw std::out_of_range(table->m_name + " do not contains a record with id " + std::to_string(id));
    }

    auto relationIt = recordIt->second.find(key);

    if (relationIt == recordIt->second.end()) {
        throw std::out_of_range(table->m_name + " do not contains a column with name " + key);
    }
    //relationIt->se
    return uva::database::value(key, relationIt->second, id, table);
}

uva::database::value uva::database::basic_active_record::operator[](const std::string& key) {
    return at(key);
}

uva::database::value uva::database::basic_active_record::operator[](size_t index) {
    return at(index);
}

//END ACTIVE RECORD

//ACTIVE RECORD COLLECTION

uva::database::active_record_collection& uva::database::active_record_collection::operator<<(size_t r)
{
    m_matches.push_back(r);
    return *this;
}

//ACTIVE RECORD COLLECTION