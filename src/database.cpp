#include <database.hpp>

using basic_migration = uva::database::basic_migration;
uva_database_define_full(basic_migration, "database_migration", "s");

//BASIC CONNECTION

uva::database::basic_connection* uva::database::basic_connection::s_connection = nullptr;

uva::database::basic_connection::basic_connection()
{
    s_connection = this;
}

uva::database::basic_connection* uva::database::basic_connection::get_connection()
{
    return s_connection;
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
    std::string error_report;
    std::string sql;

    auto elapsed = uva::diagnostics::measure_function([&]{
        /* Create SQL statement */

        sql = "CREATE TABLE IF NOT EXISTS " + table->m_name + "("  \
        "id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL";

        for(const auto& col : table->m_columns)
        {
            sql += ", " + col.first + " " + col.second;
        }

        sql += ", updated_at INTEGER DEFAULT (STRFTIME('%s')), created_at INTEGER DEFAULT (STRFTIME('%s')), removed INTEGER DEFAULT 0";

        sql += ");";

        /* Execute SQL statement */

        char* error_msg = nullptr;
        int error = sqlite3_exec(m_database, sql.c_str(),
            [](void *NotUsed, int columnCount, char **columnValue, char **value) {
            return 0;
        }, 0, &error_msg);

        if (error) {
            error_report = error_msg;
            sqlite3_free(error_msg);
        }
    });

    uva::console::color_code color_code;

    if(error_report.empty()) {
        color_code = uva::console::color_code::blue;
    } else {
        color_code = uva::console::color_code::red;
    }

    #ifdef USE_FMT_FORMT
        std::string result = std::format("({} ms) {}", std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), sql);
    #else
        std::string result = std::format("({}) {}", std::chrono::duration_cast<std::chrono::milliseconds>(elapsed), sql);
    #endif

    std::cout << uva::console::color(color_code) << result << std::endl;

    if(!error_report.empty()) {
        std::cout << uva::console::color(uva::console::color_code::red) << error_report << std::endl;
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

    int error = sqlite3_prepare_v2(m_database, sql.c_str(), (int)sql.size(), &stmt, nullptr);

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

//END SQLITE3 CONNECTION

//MULTIPLE_VALUE_HOLDER

uva::database::multiple_value_holder::multiple_value_holder()
    : type(uva::database::multiple_value_holder::value_type::null)
{

}

uva::database::multiple_value_holder::multiple_value_holder(uva::database::multiple_value_holder&& other)
    : type(other.type), str(std::move(other.str)), integer(other.integer), real(other.real), array(std::move(other.array))
{

}

uva::database::multiple_value_holder::multiple_value_holder(const uint64_t& _integer)
    : integer(_integer), type(uva::database::multiple_value_holder::value_type::integer)
{

}

uva::database::multiple_value_holder::multiple_value_holder(const int& _integer)
    : integer(_integer), type(uva::database::multiple_value_holder::value_type::integer)
{

}

uva::database::multiple_value_holder::multiple_value_holder(const time_t& _integer)
    : integer(_integer), type(uva::database::multiple_value_holder::value_type::integer)
{

}


uva::database::multiple_value_holder::multiple_value_holder(const std::string& _str)
    : str(_str), type(uva::database::multiple_value_holder::value_type::string)
{

}

uva::database::multiple_value_holder::multiple_value_holder(const char* _str)
    : str(_str), type(uva::database::multiple_value_holder::value_type::string)
{

}

uva::database::multiple_value_holder::multiple_value_holder(const bool& boolean)
    : integer(boolean), type(uva::database::multiple_value_holder::value_type::integer)
{

}

uva::database::multiple_value_holder::multiple_value_holder(const double& d)
    : real(d), type(uva::database::multiple_value_holder::value_type::real)
{
    
}

bool uva::database::multiple_value_holder::is_null() const
{
    return type == value_type::null;
}

std::string uva::database::multiple_value_holder::to_s() const
{
    switch(type)
    {
        case value_type::string:
            return str;
        break;
        case value_type::integer:
            return std::to_string(integer);
        case value_type::real:
            return std::format("{}", real);
        break;
    }

    throw std::runtime_error(std::format("failed to convert from (value_type){} to string", (size_t)type));
    return "";
}

int64_t uva::database::multiple_value_holder::to_i() const
{
    switch(type)
    {
        case value_type::string:
        {
            std::string string = str;
            bool negative = false;

            if(string.starts_with('-')) {
                negative = true;
                string.erase(0);
            }

            for(const char& c : string) {
                if(!isdigit(c)) {
                    throw std::runtime_error(std::format("invalid character '{}' in string \"{}\" while converting to integer", c, str));
                }
            }

            int64_t _integer = std::stoll(string);
            if(negative) {
                _integer *= -1;
            }

            return _integer;
        }
        break;
        case value_type::real:
            return (int)real;
        break;
        case value_type::integer:
            return integer;
        break;
    }

    throw std::runtime_error(std::format("failed to convert from (value_type){} to integer", (size_t)type));
    return -1;
}

uva::database::multiple_value_holder::operator int() const
{
    return (int)integer;
}

uva::database::multiple_value_holder::operator uint64_t() const
{
    return (uint64_t)integer;
}

uva::database::multiple_value_holder::operator int64_t() const
{
    return (int64_t)integer;
}

uva::database::multiple_value_holder::operator std::string() const
{
    return str;
}

uva::database::multiple_value_holder::operator bool() const
{
    return (bool)integer;
}

uva::database::multiple_value_holder::operator double() const
{
    return real;
}

uva::database::multiple_value_holder& uva::database::multiple_value_holder::operator=(const char* c)
{
    str = c;
    type = uva::database::multiple_value_holder::value_type::string;
    return *this;
}

uva::database::multiple_value_holder& uva::database::multiple_value_holder::operator=(const unsigned char* c)
{
    str = (char*)c;
    type = uva::database::multiple_value_holder::value_type::string;
    return *this;
}

uva::database::multiple_value_holder& uva::database::multiple_value_holder::operator=(const uint64_t& i)
{
    integer = (int64_t)i;
    type = uva::database::multiple_value_holder::value_type::integer;
    return *this;
}

uva::database::multiple_value_holder& uva::database::multiple_value_holder::operator=(const int64_t& i)
{
    integer = i;
    type = uva::database::multiple_value_holder::value_type::integer;
    return *this;
}

uva::database::multiple_value_holder& uva::database::multiple_value_holder::operator=(const std::string& s)
{
    str = s;
    type = uva::database::multiple_value_holder::value_type::string;
    return *this;
}

uva::database::multiple_value_holder& uva::database::multiple_value_holder::operator=(const bool& b)
{
    integer = (int64_t)b;
    type = uva::database::multiple_value_holder::value_type::integer;
    return *this;
}

uva::database::multiple_value_holder& uva::database::multiple_value_holder::operator=(const double& d)
{
    real = d;
    type = uva::database::multiple_value_holder::value_type::real;
    return *this;
}

uva::database::multiple_value_holder& uva::database::multiple_value_holder::operator=(const multiple_value_holder& other)
{
    switch(other.type) {
        case value_type::integer:
            integer = other.integer;
            break;
        case value_type::string:
            str = other.str;
            break;
    }
    type = other.type;
    return *this;
}

bool uva::database::multiple_value_holder::operator==(const double& d) const
{
    return d == real;
}

bool uva::database::multiple_value_holder::operator==(const std::string& s) const
{
    return str == s;
}

bool uva::database::multiple_value_holder::operator==(const bool& b) const
{
    return b == (bool)integer;
}

bool uva::database::multiple_value_holder::operator!=(const std::string& s) const
{
    return str != s;
}

bool uva::database::multiple_value_holder::operator!=(const int& i) const
{
    return integer != i;
}

bool uva::database::multiple_value_holder::operator<(const double& d) const
{
    return real < d;
}

const uva::database::multiple_value_holder& uva::database::multiple_value_holder::operator[](const size_t& i) const
{
    return array[i];
}

uva::database::multiple_value_holder& uva::database::multiple_value_holder::operator[](const size_t& i)
{
    return array[i];
}

//END MULTIPLE_VALUE_HOLDER

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

    uva::database::basic_connection::get_connection()->add_column(this, name, type, default_value);

}

void uva::database::table::change_column(const std::string& name, const std::string& type) {

    uva::database::basic_connection::get_connection()->change_column(this, name, type);

}

//TABLE

uva::database::table::table(const std::string& name, const std::vector<std::pair<std::string, std::string>>& cols)
    : m_name(name), m_columns(cols)
{
    uva::database::table::add_table(this);
}

uva::database::table::table(const std::string& name)
    : m_name(name)
{
    uva::database::table::add_table(this);
}

std::map<std::string, uva::database::table*>& uva::database::table::get_tables()
{
    static std::map<std::string, table*> s_tables;
    return s_tables;
}

void uva::database::table::add_table(uva::database::table* table)
{
    auto tables = get_tables();
    tables.insert({table->m_name, table});
}

uva::database::table* uva::database::table::get_table(const std::string& name) {

    auto tables = get_tables();

    auto it = tables.find(name);

    if (it == tables.end()) {

        if(name == "database_migrations") {
            uva::database::table * info_table = new uva::database::table("database_migrations",
            {
                { "title", "TEXT" },
            });

            //The table is only created if not exists
            uva::database::basic_connection::get_connection()->create_table(info_table);

            return info_table;
        } else {
            return new uva::database::table(name);
        }

        //throw
        return nullptr;
    }

    return it->second;
}

size_t uva::database::table::create(const std::map<std::string, uva::database::multiple_value_holder>& relations)
{
    auto keys_values = uva::string::split(relations);

    auto relation = uva::database::active_record_relation(this).insert(keys_values.second).columns(keys_values.first).into(m_name).returning("id").unscoped();
    relation.commit_without_prepare(relation.to_sql());

    return relation[0]["id"];
}

void uva::database::table::create(std::vector<std::map<std::string, std::string>>& relations)
{
    size_t id = 1;
    auto last = m_relations.rbegin();
    if (last != m_relations.rend()) {
        id = last->first + 1;
    }
}

size_t uva::database::table::create() {
    std::map<std::string, std::string> empty_relations;
    for (const auto& row : m_columns) {
        empty_relations.insert({ row.first, { "" } });
    }
    //return create(empty_relations);
    return 0;
}

void uva::database::table::destroy(size_t id) {

}

bool uva::database::table::relation_exists(size_t id) const {
    return m_relations.find(id) != m_relations.end();
}

size_t uva::database::table::find(size_t id) const {
    if (id == std::string::npos) return std::string::npos;
    auto it = m_relations.find(id);

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

size_t uva::database::table::last() {
    auto it = m_relations.rbegin();
    return it != m_relations.rend() ? it->first : std::string::npos;
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

void uva::database::table::update(size_t id, const std::string& key, const std::string& value) {
    
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

uva::database::basic_active_record::basic_active_record(const std::map<std::string, uva::database::multiple_value_holder>& value)
    : values(value)
{

}

bool uva::database::basic_active_record::present() const {
    bool present = !values.empty();
    return present;
}

void uva::database::basic_active_record::destroy() {
    get_table()->destroy(id);
    id = -1;
}

uva::database::multiple_value_holder& uva::database::basic_active_record::at(const std::string& str)
{
    auto it = values.find(str);

    if(it == values.end()) {
        throw std::runtime_error(std::format("Record from {} has no column named {}", get_table()->m_name, str));
    }

    return it->second;
}

const uva::database::multiple_value_holder& uva::database::basic_active_record::at(const std::string& str) const
{
    auto it = values.find(str);

    if(it == values.end()) {
        throw std::runtime_error(std::format("Record from {} has no column named {}", get_table()->m_name, str));
    }

    return it->second;
}

uva::database::multiple_value_holder& uva::database::basic_active_record::operator[](const std::string& str) {
    return at(str);
}

const uva::database::multiple_value_holder& uva::database::basic_active_record::operator[](const std::string& str) const {
    return at(str);
}

uva::database::multiple_value_holder& uva::database::basic_active_record::operator[](const char* str)
{
    return at(std::string(str));
}

const uva::database::multiple_value_holder& uva::database::basic_active_record::operator[](const char* str) const
{
    return at(std::string(str));
}

void uva::database::basic_active_record::save()
{
    auto relation = uva::database::active_record_relation(get_table());
//     UPDATE table_name
// SET column1 = value1, column2 = value2, ...
// WHERE condition; 
//     a   'll().where("id={}", at("id")).
}

void uva::database::basic_active_record::update(const std::string& col, const uva::database::multiple_value_holder& value)
{

}

//END ACTIVE RECORD

//MIGRATION


//END MIGRATION

//ACTIVE RECORD RELATION

uva::database::active_record_relation::active_record_relation(uva::database::table* table)
    : m_table(table)
{
    
}

uva::database::active_record_relation& uva::database::active_record_relation::update(const std::map<std::string, multiple_value_holder>& update)
{
    m_update = update;

    return *this;
}

uva::database::active_record_relation& uva::database::active_record_relation::select(const std::string& select)
{
    m_select = select;

    return *this;
}

uva::database::active_record_relation& uva::database::active_record_relation::from(const std::string& from)
{
    m_from = from;

    return *this;
}

uva::database::active_record_relation& uva::database::active_record_relation::order_by(const std::string& order)
{
    m_order = order;

    return *this;
}

uva::database::active_record_relation& uva::database::active_record_relation::limit(const std::string& limit)
{
    for(const char& c : limit) {
        if(!isdigit(c)) {
            //throw exception
            return *this;
        }
    }

    m_limit = limit;
    return *this;
}

uva::database::active_record_relation& uva::database::active_record_relation::limit(const size_t& limit)
{
    m_limit = std::to_string(limit);
    return *this;
}

uva::database::active_record_relation& uva::database::active_record_relation::insert(std::vector<uva::database::multiple_value_holder>& insert)
{
    for(auto& holder : insert) {
        if(holder.type == uva::database::multiple_value_holder::value_type::string) {
            std::string str;
            str.reserve(holder.str.size()+2);
            str.push_back('\'');

            for(const char& c : holder.str)
            {
                str.push_back(c);
                if(c == '\'') {
                    str.push_back('\'');
                }
            }


            str.push_back('\'');
            holder.str = str;
        }
    }

    m_insert.push_back(insert);

    return *this;
}

uva::database::active_record_relation& uva::database::active_record_relation::insert(std::vector<std::vector<uva::database::multiple_value_holder>>& _insert)
{
    m_insert.reserve(_insert.size());

    for(auto& values : _insert)
    {
        insert(values);
    }

    return *this;
}

uva::database::active_record_relation& uva::database::active_record_relation::columns(const std::vector<std::string>& columns)
{
    m_columns = columns;
    return *this;
}

uva::database::active_record_relation& uva::database::active_record_relation::into(const std::string& into)
{
    m_into = into;
    return *this;
}

uva::database::active_record_relation& uva::database::active_record_relation::returning(const std::string& returning)
{
    m_returning = returning;
    return *this;
}

std::vector<uva::database::multiple_value_holder> uva::database::active_record_relation::pluck(const std::string& col)
{
    std::vector<uva::database::multiple_value_holder> values;

    uva::database::active_record_relation rel = *this;

    rel.select(col);

    rel.commit();

    values.reserve(rel.m_results.size());

    for(const auto& value : rel.m_results)
    {
        if(value.size() == 1) {
            values.push_back(std::move(value[0]));
        } else if(value.size() > 1) {
            uva::database::multiple_value_holder v;
            v.type = uva::database::multiple_value_holder::value_type::array;
            v.array = std::move(value);

            values.push_back(std::move(v));
        }
    }

    return values;
}

std::vector<std::vector<uva::database::multiple_value_holder>> uva::database::active_record_relation::pluckm(const std::string& cols)
{
    std::vector<std::vector<uva::database::multiple_value_holder>> values;

    uva::database::active_record_relation rel = *this;

    rel.select(cols);

    rel.commit();

    return rel.m_results;
}

size_t uva::database::active_record_relation::count(const std::string& count) const
{
    uva::database::active_record_relation count_relation = *this;

    count_relation.m_select = "COUNT(" + count + ")";

    count_relation.commit();

    try {
        auto first_row = count_relation.m_results.begin();

        if(first_row == count_relation.m_results.end()) {
            return 0;
        }

        auto first_col = first_row->begin();

        if(first_col == first_row->end()) {
            return 0;
        }

        return *first_col;

    } catch(std::exception& e) {

    }

    return 0;
}

std::map<std::string, uva::database::multiple_value_holder> uva::database::active_record_relation::first()
{
    uva::database::active_record_relation first_relation = *this;

    if(!first_relation.m_order.size()) {
        first_relation.order_by("id");
    }

    first_relation.limit(1);

    if(first_relation.empty()) {
        return std::map<std::string, uva::database::multiple_value_holder>();
    }

    return first_relation[0];
}

void uva::database::active_record_relation::each_with_index(std::function<void(std::map<std::string, uva::database::multiple_value_holder>& value, const size_t&)> func)
{
    size_t index = 0;
    size_t last_id = 0;

    while(true) {
        uva::database::active_record_relation copy = uva::database::active_record_relation(*this);

        if(index) {
            copy.where("{} > {}", m_order || "id", last_id).first();
        }

        auto first = copy.first();

        if(first.empty()) {
            return;
        } else {
            last_id = first[m_order || "id"];

            func(first, index);
        }

        ++index;
    }
}

void uva::database::active_record_relation::each(std::function<void(std::map<std::string, uva::database::multiple_value_holder>&)> func)
{
    each_with_index([&](std::map<std::string, uva::database::multiple_value_holder>& value, const size_t& index){
        func(value);
    });
}

bool uva::database::active_record_relation::empty()
{
    commit();
    return m_results.empty();
}

std::map<std::string, uva::database::multiple_value_holder> uva::database::active_record_relation::operator[](const size_t& index)
{
    std::map<std::string, uva::database::multiple_value_holder> result;

    auto row = m_results[index];

    auto col_it = row.begin();
    for(const std::string& col : m_columnsNames) {
        result.insert({col,*col_it});
        col_it++;
    }

    return result;
}

uva::database::active_record_relation uva::database::active_record_relation::unscoped()
{
    uva::database::active_record_relation unscoped = *this;

    unscoped.m_unscoped = true;

    return unscoped;
}

void uva::database::active_record_relation::append_where(const std::string& where)
{
    //TODO: Remove previous columns values in m_where

    if(m_where.size()) {
        m_where += "AND " + where;
    } else {
        m_where = where;
    }
}

std::string uva::database::active_record_relation::commit_sql() const
{
    std::string sql = "";

    if(m_update.size()) {
        sql += "UPDATE " + m_table->m_name + " SET ";

        sql += uva::string::join(
            uva::string::join(m_update, [](const auto& val) {
                std::string val_str = val.second.to_s();
                if(val.second.type == uva::database::multiple_value_holder::value_type::string) {
                    val_str = uva::string::prefix_sufix(val_str, "'", "'");
                }
                return std::format("{}={}", val.first, val_str);
            }),
        ',');
    }

    if(m_select.size()) {
        sql += "SELECT " + m_select;
    }

    if(m_from.size()) {
        sql += " FROM " + m_from;
    }

    if(m_where.size()) {
        sql += " WHERE " + m_where;
    }

    if(!m_unscoped) {
        sql += (m_where.size() ? " AND " : " WHERE ") + (std::string)"removed = 0";
    }

    if(m_order.size()) {
        sql += " ORDER BY " + m_order;
    }

    if(m_limit.size()) {
        sql += " LIMIT " + m_limit;
    }

    if(m_insert.size()) {
        sql += " INSERT INTO " + m_into || m_from || m_table->m_name;

        if(m_columns.size())
        {
            sql += "(";

            sql += uva::string::join(m_columns, ',');

            sql += ")";
        }

        sql += " VALUES ";
        for(size_t i = 0; i < m_insert.size(); ++i)
        {
            sql  += "(";

            sql += uva::string::join(m_insert[i], ',');

            sql += ")";

            if(i < m_insert.size()-1) {
                sql += ", ";
            }
        }
    }

    if(m_returning.size())
    {
        sql += " RETURNING " + m_returning;
    }

    sql += ";";

    return sql;
}

void uva::database::active_record_relation::commit()
{
    std::string sql = commit_sql();

    commit(sql);
}

std::string uva::database::active_record_relation::to_sql() const
{
    std::string sql = commit_sql();

    return sql;
}

void uva::database::active_record_relation::commit_without_prepare(const std::string& sql)
{
    m_columnsNames.clear();
    m_columnsIndexes.clear();
    m_columnsTypes.clear();

    sqlite3_connection* connection = (sqlite3_connection*)uva::database::basic_connection::get_connection();

    char* error_msg = nullptr;
    struct callback_data {
        bool firstCalback = true;
        uva::database::active_record_relation* self;
    };

    callback_data data;
    data.self = this;

    int error = 0;
    auto elapsed = uva::diagnostics::measure_function([&]
    {
        error = sqlite3_exec(connection->get_database(), sql.c_str(), [](void *pdata, int columnCount, char **columnValue, char **columnNames) {

        callback_data* data = (callback_data*)pdata;
        std::vector<uva::database::multiple_value_holder> cols;

        for(size_t i = 0; i < columnCount; ++i) {

            if(data->firstCalback) {
                std::string colName = columnNames[i];
                
                data->self->m_columnsNames.push_back(colName);
                data->self->m_columnsIndexes.insert({colName, i});

                data->self->m_columnsTypes.push_back(uva::database::multiple_value_holder::value_type::string);
            }
        
            data->firstCalback = false;
            cols.push_back(columnValue[i]);
        }

        data->self->m_results.push_back(cols);

        return 0;
    }, &data, &error_msg);});

    std::string error_report;
    if (error) {
        error_report = error_msg;
        sqlite3_free(error_msg);
    }

    uva::console::color_code color_code;

    if(error_report.empty()) {
        color_code = uva::console::color_code::blue;
    } else {
        color_code = uva::console::color_code::red;
    }

    #ifdef USE_FMT_FORMT
        std::string result = std::format("({} ms) {}", std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), sql.size() > 1000 ? sql.substr(0, 1000) : sql);
    #else
        std::string result = std::format("({}) {}", std::chrono::duration_cast<std::chrono::milliseconds>(elapsed), sql.size() > 1000 ? sql.substr(0, 1000) : sql);
    #endif

    std::cout << uva::console::color(color_code) << result << std::endl;

    if(!error_report.empty()) {
        std::cout << uva::console::color(uva::console::color_code::red) << error_report << std::endl;
        throw std::runtime_error(error_report);
    }
}

void uva::database::active_record_relation::commit(const std::string& sql)
{
    m_columnsNames.clear();
    m_columnsIndexes.clear();
    m_columnsTypes.clear();

    std::string error_report;

    auto elapsed = uva::diagnostics::measure_function([&] {

        sqlite3_stmt* stmt;

        sqlite3_connection* connection = (sqlite3_connection*)uva::database::basic_connection::get_connection();

        int error = sqlite3_prepare_v2(connection->get_database(), sql.c_str(), (int)sql.size(), &stmt, nullptr);

        char* error_msg = nullptr;

        if(error) {
            error_msg = (char*)sqlite3_errmsg(connection->get_database());
        }
 
        if(error) {
            error_report = error_msg;
            return;
        }

        size_t colCount = sqlite3_column_count(stmt);

        for (int colIndex = 0; colIndex < colCount; colIndex++) {        

            std::string name = sqlite3_column_name(stmt, colIndex);
            const char* t = sqlite3_column_decltype(stmt, colIndex);

            if(t == nullptr) {
                if(name.starts_with("COUNT")) {
                    t = "INTEGER";
                }
            }

            std::string type = t ? t : "TEXT";

            m_columnsNames.push_back(name);
            m_columnsIndexes.insert({name, colIndex});

            uva::database::multiple_value_holder::value_type value_type;

            if(type == "TEXT" || type.starts_with("NVARCHAR")) {
                value_type = uva::database::multiple_value_holder::value_type::string;
            } else if(type == "REAL") {
                value_type = uva::database::multiple_value_holder::value_type::real;
            }
            else {
                value_type = uva::database::multiple_value_holder::value_type::integer;
            }

            m_columnsTypes.push_back(value_type);
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            auto current_col = m_columnsTypes.begin();
            std::vector<uva::database::multiple_value_holder> cols;

            for (int colIndex = 0; colIndex < colCount; colIndex++) {

                uva::database::multiple_value_holder holder;
                holder.type = *current_col;

                switch (*current_col)
                {

                    case uva::database::multiple_value_holder::value_type::string: {
                        const unsigned char* value = sqlite3_column_text(stmt, colIndex);
                        if(!value) {
                            value = (const unsigned char*)"";
                        }
                        holder = value;
                    }
                    break;
                    case uva::database::multiple_value_holder::value_type::integer: {
                        int64_t value = sqlite3_column_int64(stmt, colIndex);
                        holder = value;
                    }
                    break;
                    case uva::database::multiple_value_holder::value_type::real: {
                        double value = sqlite3_column_double(stmt, colIndex);
                        holder = value;
                    }
                    break;
                }

                cols.emplace_back(holder);

                current_col++;
            }

            m_results.emplace_back(cols);
        }

        //  Step, Clear and Reset the statement after each bind.
        error = sqlite3_step(stmt);
        error = sqlite3_clear_bindings(stmt);
        error = sqlite3_reset(stmt);    

        if (error) {        
            //throw std::runtime_error(sqlite3_errmsg(m_database));
        }

        sqlite3_finalize(stmt);

        if (error) {
            //std::string error_report = error_msg;
            //sqlite3_free(error_msg);
            //throw std::runtime_error(""/*error_report*/);
        }    
    });

    uva::console::color_code color_code;

    if(error_report.empty()) {
        color_code = uva::console::color_code::blue;
    } else {
        color_code = uva::console::color_code::red;
    }

    #ifdef USE_FMT_FORMT
        std::string result = std::format("({} ms) {}", std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), sql.size() > 1000 ? sql.substr(0, 1000) : sql);
    #else
        std::string result = std::format("({}) {}", std::chrono::duration_cast<std::chrono::milliseconds>(elapsed), sql.size() > 1000 ? sql.substr(0, 1000) : sql);
    #endif

    std::cout << uva::console::color(color_code) << result << std::endl;

    if(!error_report.empty()) {
        std::cout << uva::console::color(uva::console::color_code::red) << error_report << std::endl;
        throw std::runtime_error(error_report);
    }
}

//ACTIVE RECORD RELATION

//BASIC MIGRATION

uva::database::basic_migration::basic_migration(const std::string& __title)
    : title(__title), uva::database::basic_active_record(0)
{
    get_migrations().push_back(this);
}

 std::string uva::database::basic_migration::make_label()
 {
    time_t date = at("created_at");

    tm* local_time = std::localtime(&date);

    char buffer[100];

    std::strftime(buffer, 100, "%d%m%Y%H%M%S", local_time);

    std::string date_str = buffer;

    return date_str + "_" + title;
}

bool uva::database::basic_migration::is_pending() {
    return !find_by("title='{}'", title).present();
}

// uva::database::table* uva::database::basic_migration::get_table()
// {
//     static uva::database::table* info_table = nullptr;
    
//     if(!info_table) {
//         info_table = new uva::database::table("database_migrations",
//         {
//             { "title", "TEXT" },
//             { "date",  "INTEGER"}
//         });

//         //The table is only created if not exists
//         uva::database::basic_connection::get_connection()->create_table(info_table);
//     }

//     return info_table;
// }

void uva::database::basic_migration::call_change()
{
    try {
        auto elapsed = uva::diagnostics::measure_function([&](){
            this->change();
            this->apply();
        });

        #ifdef USE_FMT_FORMT
            std::cout << uva::console::color(uva::console::color_code::green) << std::format("{}: {} ms...", this->title, std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()) << std::endl;
        #else
            std::cout << uva::console::color(uva::console::color_code::green) << std::format("{}: {}...", this->title, std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)) << std::endl;
        #endif

    } catch(std::exception e)
    {
        std::cout << uva::console::color(uva::console::color_code::red) << std::format("{}: {}\n\nAborting...", this->title, e.what()) << std::endl;
        exit(EXIT_FAILURE);

        return;
    }
}

std::vector<uva::database::basic_migration*>& uva::database::basic_migration::get_migrations()
{
    static std::vector<uva::database::basic_migration*> s_migrations;

    return s_migrations;
}

void uva::database::basic_migration::do_pending_migrations()
{
    std::vector<uva::database::basic_migration*>& migrations = uva::database::basic_migration::get_migrations();
    for(uva::database::basic_migration* migration : migrations)
    {
        if(migration->is_pending()) {
            migration->call_change();
        }
    }
}

void uva::database::basic_migration::apply()
{
    size_t id = create({
        { "title",   title }
    });
}

void uva::database::basic_migration::add_table(const std::string& table_name, const std::vector<std::pair<std::string, std::string>>& cols)
{
    uva::database::table* new_table = new uva::database::table(table_name, cols);
    uva::database::basic_connection::get_connection()->create_table(new_table);
}

void uva::database::basic_migration::drop_table(const std::string& table_name)
{
    uva::database::active_record_relation().commit_without_prepare(std::format("DROP TABLE {};", table_name));
}

void uva::database::basic_migration::add_index(const std::string& table_name, const std::string& column)
{
    uva::database::active_record_relation().commit_without_prepare(std::format("CREATE INDEX idx_{}_on_{} ON {}({});", table_name, uva::string::replace(column, ',', '_'), table_name, column));
}

void uva::database::basic_migration::add_column(const std::string& table_name, const std::string& name, const std::string& type, const std::string& default_value) const
{
    uva::database::table* table = uva::database::table::get_table(table_name);
    table->add_column(name, type, default_value);
}

void uva::database::basic_migration::change_column(const std::string& table_name, const std::string& name, const std::string& type) const
{
    uva::database::table* table = uva::database::table::get_table(table_name);
    table->change_column(name, type);
}

//END BASIC MIGRATION