#include <database.hpp>

//BASIC CONNECTION

std::map<std::string, uva::database::basic_connection*>& uva::database::basic_connection::get_connections() {
    static std::map<std::string, uva::database::basic_connection*> connections;
    return connections;
}

uva::database::basic_connection* uva::database::basic_connection::get_connection(const std::string& connection) {
    auto& connections = uva::database::basic_connection::get_connections();

    auto it = connections.find(connection);

    if (it == connections.end()) return nullptr;

    return it->second;
}

void uva::database::basic_connection::add_connection(const std::string& index, uva::database::basic_connection* connection) {

    auto& connections = uva::database::basic_connection::get_connections();
    connections.insert({ index, connection });

}

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

    for(const auto& col : table->m_columns)
    {
        sql += ", " + col.first + " " + col.second;
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
    table->m_columns.clear();
    table->m_relations.clear();

    std::string sql = "SELECT * FROM " + table->m_name + ";";
    sqlite3_stmt* stmt;

    int error = sqlite3_prepare_v2(m_database, sql.c_str(), sql.size(), &stmt, nullptr);

    size_t colCount = sqlite3_column_count(stmt);

    std::vector<std::string> indexCol;
    std::vector<bool> indexText;

    for (int colIndex = 0; colIndex < colCount; colIndex++) {        

        std::string type = sqlite3_column_decltype(stmt, colIndex);
        std::string name = sqlite3_column_name(stmt, colIndex);

        if (type == "INT") {
            type = "INTEGER";
        }

        indexCol.push_back(name);        
        indexText.push_back(type != "INTEGER");

        //Skip ID column
        if (colIndex) table->m_columns.push_back({ name, type });
    }
      
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        size_t id;
        for (int colIndex = 0; colIndex < colCount; colIndex++) {

            if (!colIndex) {
                id = sqlite3_column_int(stmt, colIndex);
            }
            else {
                const unsigned char* value = sqlite3_column_text(stmt, colIndex);

                table->m_relations[id].insert({ indexCol[colIndex], std::string((const char*)value) });
            }
        }
    }
    //  Step, Clear and Reset the statement after each bind.
    error = sqlite3_step(stmt);
    error = sqlite3_clear_bindings(stmt);
    error = sqlite3_reset(stmt);    

    if (error) {        
        //throw std::runtime_error(sqlite3_errmsg(m_database));
    }

    sqlite3_finalize(stmt);

    char* error_msg = nullptr;

    if (error) {
        //std::string error_report = error_msg;
        //sqlite3_free(error_msg);
        //throw std::runtime_error(""/*error_report*/);
    }    
}

void uva::database::sqlite3_connection::alter_table(uva::database::table* table, const std::string& new_signature)
{
    std::string cols = "ID";

    for (const auto& row : table->m_columns)
    {
        cols += ", " + row.first;
    }

    std::string oldTableName = table->m_name + "_old";

    char* error_msg = nullptr;
    std::string sql =

        "PRAGMA foreign_keys = off;"
        "BEGIN TRANSACTION;"
        "ALTER TABLE " + table->m_name + " RENAME TO " + oldTableName + ";"
        "CREATE TABLE " + new_signature + ";"
        "INSERT INTO " + table->m_name + " (" + cols + ") SELECT " + cols + " FROM " + oldTableName + ";"
        "DROP TABLE " + oldTableName + ";"
        "COMMIT; "
        "PRAGMA foreign_keys = on;";

    int error = sqlite3_exec(m_database, sql.c_str(), nullptr, nullptr, &error_msg);
    if (error) {
        std::string error_report = error_msg;
        sqlite3_free(error_msg);
        throw std::runtime_error(error_report);
    }
}

bool uva::database::sqlite3_connection::insert(table* table, size_t id, const std::map<std::string, std::string>& relations) {
    char* error_msg = nullptr;
    std::string sql = "INSERT INTO " + table->m_name + " VALUES(" + std::to_string(id);
    
    for(const auto& col : table->m_columns) {
        sql += ", \"";
        auto it = relations.find(col.first);
        if(it != relations.end()) {
            if (col.second == "TEXT") {
                sql.reserve(sql.size() + it->second.size());
                for (const char& c : it->second) {
                    if (c == '\"') {
                        sql.push_back('\"');
                    }
                    sql.push_back(c);
                }                
            }
            else {
                sql += it->second;
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

bool uva::database::sqlite3_connection::insert(table* table, size_t id, const std::vector<std::map<std::string, std::string>>& relations)
{
    //todo: merge those functions
    char* error_msg = nullptr;
    std::string sql = "INSERT INTO " + table->m_name + " VALUES\n";
        
    for (const auto& relation : relations) {
        sql += "(" + std::to_string(id++);
        for (const auto& col : table->m_columns) {
            sql += ", \"";
            auto it = relation.find(col.first);
            if (it != relation.end()) {
                if (col.second == "TEXT") {
                    sql.reserve(sql.size() + it->second.size());
                    for (const char& c : it->second) {
                        if (c == '\"') {
                            sql.push_back('\"');
                        }
                        sql.push_back(c);
                    }
                }
                else {
                    sql += it->second;
                }
            }
            sql += "\"";
        }
        sql += "),\n";
    }

    sql[sql.size() - 2] = ';';
    
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

void uva::database::sqlite3_connection::add_column(uva::database::table* table, const std::string& name, const std::string& type, const std::string& default_value)
{
    std::string cols_definitions = "ID INT PRIMARY KEY NOT NULL";

    for (const auto& row : table->m_columns)
    {
        cols_definitions += ", " + row.first + " " + row.second;
    }

    cols_definitions += ", " + name + " " + type + " DEFAULT \"" + default_value + "\"";

    std::string new_definition = table->m_name + "(" + cols_definitions + ")";

    alter_table(table, new_definition);

    for (auto it = table->m_relations.begin(); it != table->m_relations.end(); ++it) {
        it->second.insert({ name, default_value });
    }
}

void uva::database::sqlite3_connection::change_column(uva::database::table* table, const std::string& name, const std::string& type)
{
    std::string cols_definitions = "ID INT PRIMARY KEY NOT NULL";

    auto it = table->find_column(name);

    it->second = type;

    for (const auto& row : table->m_columns)
    {
        cols_definitions += ", " + row.first + " " + row.second;
    }    

    std::string new_definition = table->m_name + "(" + cols_definitions + ")";

    alter_table(table, new_definition);
}

uva::database::sqlite3_connection* uva::database::sqlite3_connection::connect(const std::filesystem::path& database)
{
    uva::database::basic_connection* connection = uva::database::basic_connection::get_connection(database.string());
    if (!connection) {
        connection = new uva::database::sqlite3_connection(database);

        add_connection(database.string(), connection);
    }

    return (uva::database::sqlite3_connection*)connection;
}

//END SQLITE3 CONNECTION

//VALUE

uva::database::value::value(const std::string& key, std::string& value, size_t id, table* table)
    : m_key(key), m_value(value), m_recordId(id), m_table(table)
{

}

bool uva::database::value::present() {
    return m_value.size();
}

uva::database::value::operator std::string &() {
    return m_value;
}

uva::database::value::operator size_t() {

    auto it = m_table->find_column(m_key);

    if (it->second == "INTEGER") {
        bool valid = true;
        for (size_t i = 0; i < m_value.size(); ++i) {
            if (!isdigit(m_value[i])) {
                if (i == 0 && m_value[0] == '-') {
                    continue;
                }
                else {
                    valid = false;
                    break;
                }
            }
        }

        if (valid) {
            //throws exception
            return std::stoll(m_value);
        }

        if (m_value == "TRUE") {
            return 1;
        }
        else if (m_value == "FALSE") {
            return 0;
        }
    }
    throw std::bad_cast();
}

uva::database::value& uva::database::value::operator=(const size_t& value) {
    std::string other;
    if (m_value == "TRUE" || m_value == "FALSE") {
        other = value ? "TRUE" : "FALSE";
    }
    else {
        other = std::to_string(value);
    }
    m_table->update(m_recordId, m_key, other);
    m_value = other;
    return *this;
}

uva::database::value& uva::database::value::operator=(const std::string& other)
{
    m_table->update(m_recordId, m_key, other);
    m_value = other;
    return *this;
}

bool uva::database::value::operator== (const std::string& other) {
    return m_value == other;
}
bool uva::database::value::operator!=(const std::string& other) {
    return m_value != other;
}
bool uva::database::value::operator== (const bool& other) {
    auto it = m_table->find_column(m_key);

    if (it->second == "INTEGER") {
        uint64_t value = (uint64_t)*this;
        return value == other;
    }
    else {
        throw std::bad_cast();
    }
}

std::string& uva::database::table::at(size_t id, const std::string& key) {
    auto it = m_relations.find(id);

    if (it == m_relations.end()) {
        throw std::out_of_range(m_name + " do not contains a record with id " + std::to_string(id));
    }

    auto colIt = it->second.find(key);

    if (colIt == it->second.end()) {
        throw std::out_of_range(m_name + " do not contains a column with name " + key);
    }

    return colIt->second;
}

void uva::database::table::add_column(const std::string& name, const std::string& type, const std::string& default_value) {

    m_connection->add_column(this, name, type, default_value);

}

void uva::database::table::change_column(const std::string& name, const std::string& type) {

    m_connection->change_column(this, name, type);

}

//END VALUE

//TABLE

uva::database::table::table(const std::string& name,
    const std::vector<std::pair<std::string, std::string>>& columns,
    basic_connection* connection)

    : m_connection(connection), m_columns(columns), m_name(name)
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

uva::database::table* uva::database::table::get_table(const std::string& name) {
    
    auto tables = get_tables();

    auto it = tables.find(name);

    if (it == tables.end()) {
        //throw exception?
        return nullptr;
    }

    return it->second;
}

size_t uva::database::table::create(const std::map<std::string, std::string>& relations)
{
    size_t id = 1;
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

void uva::database::table::create(std::vector<std::map<std::string, std::string>>& relations)
{
    size_t id = 1;
    auto last = m_relations.rbegin();
    if (last != m_relations.rend()) {
        id = last->first + 1;
    }

    if (m_connection->insert(this, id, relations)) {
        for (const auto& relation : relations) {
            m_relations.insert({ id++, relation });
        }
    }    
}

size_t uva::database::table::create() {
    std::map<std::string, std::string> empty_relations;
    for (const auto& row : m_columns) {
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

size_t uva::database::table::first() {
    auto it = m_relations.begin();
    return it != m_relations.end() ? it->first : std::string::npos;
}

std::vector<std::pair<std::string, std::string>>::iterator uva::database::table::find_column(const std::string& col) {
    auto it = std::find_if(m_columns.begin(), m_columns.end(), [&col](const std::pair<std::string, std::string>& pair) { return pair.first == col; });
    if (it == m_columns.end()) {        
        throw std::out_of_range("There is no column named '" + col + "' in table '" + m_name + "'");
    }
    return it;
}

std::vector<std::pair<std::string, std::string>>::const_iterator uva::database::table::find_column(const std::string& col) const {
    auto it = std::find_if(m_columns.begin(), m_columns.end(), [&col](const std::pair<std::string, std::string>& pair) { return pair.first == col; });
    if (it == m_columns.end()) {
        throw std::out_of_range("There is no column named '" + col + "' in table '" + m_name + "'");
    }
    return it;
}

uva::database::active_record_collection uva::database::table::where(const std::map<std::string, std::string>& relations)
{
    active_record_collection collection;    
    collection.m_table = this;
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

    return collection;
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

//MIGRATION


//END MIGRATION

//ACTIVE RECORD COLLECTION

uva::database::active_record_collection::active_record_collection(const uva::database::active_record_collection& collection)
    : m_matches(collection.m_matches), m_table(collection.m_table)
{

}


uva::database::active_record_collection uva::database::active_record_collection::where(const std::map<std::string, std::string>& columns)
{
    active_record_collection collection;
    collection.m_table = m_table;

    for (const auto& id : m_matches) {
        auto& record = m_table->m_relations[id];

        for (const auto& column : columns) {

            if (record[column.first] == column.second) {
                collection << id;
                break;
            }
        }
    }

    return collection;
}

uva::database::active_record_collection& uva::database::active_record_collection::operator<<(size_t r)
{
    m_matches.push_back(r);
    return *this;
}

//ACTIVE RECORD COLLECTION