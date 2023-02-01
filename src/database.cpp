#include <database.hpp>

// STATIC MEMBERS

std::map<std::string, var::var_type> uva::database::sql_values_types_map 
{
    { "NULL", var::var_type::null_type },
    { "TEXT", var::var_type::string },
    { "INTEGER", var::var_type::integer },
    { "REAL", var::var_type::real },
};

const var::var_type& uva::database::sql_delctype_to_value_type(const std::string& type)
{
    auto it = sql_values_types_map.find(type);

    if(it != sql_values_types_map.end()) 
    {
        return it->second;
    }

    throw std::runtime_error(std::format("failed to convert from '{}' to value_type", type));
}


void uva::database::within_transaction(std::function<void()> __f)
{
    uva::database::basic_connection::get_connection()->begin_transaction();
    try {
        __f();
    } catch(std::exception e)
    {
        uva::database::basic_connection::get_connection()->end_transaction();    
        throw;
    }
    uva::database::basic_connection::get_connection()->end_transaction();
}


size_t uva::database::query_buffer_lenght = uva::database::query_buffer_lenght_default;
bool   uva::database::enable_query_cout_printing = uva::database::enable_query_cout_printing_default;

// END STATIC MEMBERS

using basic_migration = uva::database::basic_migration;
uva_database_define_full(basic_migration, "database_migrations");

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

        sql = "CREATE TABLE IF NOT EXISTS " + table->m_name + "(";

        for(const auto& col : table->m_columns)
        {
            sql += col.first + " " + col.second + ", ";
        }

        sql.pop_back();
        sql.pop_back();

        sql += " );";

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

void uva::database::sqlite3_connection::begin_transaction() 
{
    active_record_relation().commit("BEGIN TRANSACTION;");
    enable_query_cout_printing = false;
}

void uva::database::sqlite3_connection::end_transaction() 
{
    enable_query_cout_printing = enable_query_cout_printing_default;
    active_record_relation().commit("END TRANSACTION;");
}

//END SQLITE3 CONNECTION

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
                { "id",         "INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL" },
                { "title",      "TEXT NOT NULL" },
                { "updated_at", "INTEGER DEFAULT (STRFTIME('%s'))" },
                { "created_at", "INTEGER DEFAULT (STRFTIME('%s'))" },
                { "removed",    "INTEGER DEFAULT 0" },    
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

size_t uva::database::table::create(const std::map<std::string, var>& relations)
{
    auto keys_values = uva::string::split(relations);

    auto relation = uva::database::active_record_relation(this).insert(keys_values.second).columns(keys_values.first).into(m_name).returning("id").unscoped();
    relation.commit_without_prepare(relation.to_sql());

    return relation[0]["id"].to_i();
}

void uva::database::table::create(std::vector<var>& relations, const std::vector<std::string>& columns)
{
    var rows = std::move(relations);

    auto relation = uva::database::active_record_relation(this).insert(rows).columns(columns).into(m_name).unscoped();
    relation.commit_without_prepare(relation.to_sql());
}

void uva::database::table::create(var& rows, const std::vector<std::string>& columns)
{
    auto relation = uva::database::active_record_relation(this).insert(rows).columns(columns).into(m_name).unscoped();
    relation.commit_without_prepare(relation.to_sql());
}

void uva::database::table::create(std::vector<std::map<std::string, var>>& relations)
{
    std::vector<std::string> keys;
    std::vector<var> values;

    for(auto& row : relations)
    {
        auto keys_values = uva::string::split(row);
        row.clear();

        if(keys.empty()) {
            keys = keys_values.first;
        } else {
            #if UVA_DEBUG_LEVEL > 3
                bool keys_match = keys.size() == keys_values.first.size() && std::equal(keys.begin(), keys.end(), keys_values.first.begin());

                if(!keys_match) {
                    throw std::runtime_error("Wrong number of arguments");
                    return;
                }
            #endif
        }

        values.insert(values.end(), keys_values.second.begin(), keys_values.second.end());
    }

    auto relation = uva::database::active_record_relation(this).insert(values).columns(keys).into(m_name).unscoped();
    relation.commit_without_prepare(relation.to_sql());
}

void uva::database::table::create(std::vector<std::vector<var>>& values, const std::vector<std::string>& columns)
{
    auto relation = uva::database::active_record_relation(this).insert(values).columns(columns).into(m_name).unscoped();
    relation.commit_without_prepare(relation.to_sql());
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

void uva::database::table::update(size_t id, const std::map<std::string, var>& values)
{
    active_record_relation(this).unscoped().update(values);
}

void uva::database::table::update(size_t id, const std::string& key, const std::string& value) {
    update(id, { { key, value } });
}

//END TABLE

//ACTIVE RECORD

uva::database::basic_active_record::basic_active_record(size_t _id)
    : id(_id) {
    
}

uva::database::basic_active_record::basic_active_record(const basic_active_record& _record)
    : id(_record.id), values(_record.values)
{

}

uva::database::basic_active_record::basic_active_record(basic_active_record&& _record)
    :  id(_record.id), values(std::move(_record.values))
{
    _record.id = 0;
}

uva::database::basic_active_record::basic_active_record(const std::map<std::string, var>& _values)
    : values(_values)
{

}

uva::database::basic_active_record::basic_active_record(std::map<std::string, var>&& _values)
    : values(std::forward<std::map<std::string, var>>(_values))
{

}

bool uva::database::basic_active_record::present() const {
    if (values.empty()) {
        return false;
    }

    auto it = values.find("id");

    if(it == values.end()) {
        return false;
    }

    return it->second;
}

void uva::database::basic_active_record::destroy() {
    get_table()->destroy(id);
    id = -1;
}

uva::database::basic_active_record& uva::database::basic_active_record::operator=(const uva::database::basic_active_record& other)
{
    values = other.values;
    id = other.id;
    
    return *this;
}

var& uva::database::basic_active_record::at(const std::string& str)
{
    var& value = values[str];
    return value;

    //return it->second;auto it = values.find(str);

    // if(it == values.end()) {
    //     throw std::runtime_error(std::format("Record from {} has no column named {}", get_table()->m_name, str));
    // }

    //return it->second;
}

const var& uva::database::basic_active_record::at(const std::string& str) const
{
    auto it = values.find(str);

    if(it == values.end()) {
        throw std::runtime_error(std::format("Record from {} has no column named {}", get_table()->m_name, str));
    }

    return it->second;
}

var& uva::database::basic_active_record::operator[](const std::string& str) {
    return at(str);
}

const var& uva::database::basic_active_record::operator[](const std::string& str) const {
    return at(str);
}

var& uva::database::basic_active_record::operator[](const char* str)
{
    return at(std::string(str));
}

const var& uva::database::basic_active_record::operator[](const char* str) const
{
    return at(std::string(str));
}

std::string uva::database::basic_active_record::to_s() const
{
    std::string joined_values = uva::string::join(uva::string::join(values, [](const std::pair<std::string, var> & v){
        return std::format("{}={}", v.first, v.second.to_s());
    }), ", ");

    return std::format("<{}Class> {{ {} }}", class_name(), joined_values);
}

void uva::database::basic_active_record::save()
{
    before_save();
    
    auto it = values.find("id"); 

    if(it == values.end() || it->second.is_null()) {
       values["id"] = get_table()->create(values);
    } else {
        before_update();
        get_table()->update(it->second, values);
    }
}

void uva::database::basic_active_record::update(const std::string& col, const var& value)
{
    values[col] = value;
    before_update();

    uva::database::table* table = get_table();
    table->update(values["id"], col, value);

    before_save();
}

void uva::database::basic_active_record::update(const std::map<std::string, var>& _values)
{
    for(const auto& value : _values) {
        values[value.first] = value.second;
    }

    before_update();
    
    get_table()->update(values["id"], _values);

    before_save();
}

void uva::database::basic_active_record::update_exposed_column(const std::string &__key, basic_active_record_column *__column)
{
    auto it = values.find(__key);

    if(it != values.end()) {
        __column->m_value_ptr = it->second.m_value_ptr;
        __column->type        = it->second.type;
    }
}

void uva::database::basic_active_record::update_exposed_columns()
{
    for(auto& value : values) {
        auto it = columns.find(value.first);
        update_exposed_column(value.first, it->second);
    }
}

void uva::database::basic_active_record::expose_column(const std::string &__key, basic_active_record_column *__column)
{
    columns[__key] = __column;

    auto it = values.find(__key);

    if(it != values.end()) {
        update_exposed_column(__key, __column);
    }
}

//END ACTIVE RECORD

//ACTIVE RECORD COLUMN

uva::database::basic_active_record_column::basic_active_record_column(const std::string &__key, basic_active_record *__record)
    : key(__key), active_record(__record)
{
    active_record->expose_column(key, this);
}

//END ACTIVE RECORD COLUMN

//MIGRATION


//END MIGRATION

//ACTIVE RECORD RELATION

uva::database::active_record_relation::active_record_relation(uva::database::table* table)
    : m_table(table)
{
    
}

void uva::database::active_record_relation::update(const std::map<std::string, var>& update)
{
    m_update = update;
    commit_without_prepare();

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

std::map<std::string, var> uva::database::active_record_relation::where(std::map<var, var> &&v)
{
    std::string where = "(";
    size_t to_reserve = v.size()*64;

    for(const auto& value : v) {
        where += value.first.to_s();
        where += " = ";

        if(value.second.type == var::var_type::string) {
            where.push_back('\'');
            where += value.second.to_s();
            where.push_back('\'');
        } else {
            where += value.second.to_s();
        }
    }

    where += " ) ";

    UVA_CHECK_RESERVED_BUFFER(where, to_reserve);

    return active_record_relation(*this).where(where).first();
}

uva::database::active_record_relation &uva::database::active_record_relation::group_by(const std::string &group)
{
    m_group = group;
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

uva::database::active_record_relation& uva::database::active_record_relation::insert(var& insert)
{
    m_insert = std::move(insert);
    return *this;
}

uva::database::active_record_relation& uva::database::active_record_relation::insert(std::vector<var>& insert)
{
    var v(std::move(insert));

    m_insert.push_back(std::move(v));
    return *this;
}

uva::database::active_record_relation& uva::database::active_record_relation::insert(std::vector<std::vector<var>>& __insert)
{
    m_insert.reserve(__insert.size());
    for(auto& insert : __insert)
    {
        var v = std::move(insert);

        m_insert.push_back(std::move(v));
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

std::vector<var> uva::database::active_record_relation::pluck(const std::string& col)
{
    std::vector<var> values;

    uva::database::active_record_relation rel = *this;

    rel.select(col);

    rel.commit();

    values.reserve(rel.m_results.size());

    for(auto& value : rel.m_results)
    {
        if(value.size() == 1) {
            values.push_back(std::move(value[0]));
            value.clear();
        } else if(value.size() > 1) {
            values.push_back(std::move(value));
        }
    }

    return values;
}

std::vector<std::vector<var>> uva::database::active_record_relation::pluckm(const std::string& cols)
{
    std::vector<std::vector<var>> values;

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

        return first_col->to_i();

    } catch(std::exception& e) {

    }

    return 0;
}

std::map<std::string, var> uva::database::active_record_relation::first()
{
    uva::database::active_record_relation first_relation = *this;

    if(!first_relation.m_order.size()) {
        first_relation.order_by("id");
    }

    first_relation.limit(1);

    if(first_relation.empty()) {
        return std::map<std::string, var>();
    }

    return first_relation[0];
}

void uva::database::active_record_relation::each_with_index(std::function<void(std::map<std::string, var>& value, const size_t&)> func)
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

void uva::database::active_record_relation::each(std::function<void(std::map<std::string, var>&)> func)
{
    each_with_index([&](std::map<std::string, var>& value, const size_t& index){
        func(value);
    });
}

bool uva::database::active_record_relation::empty()
{
    commit();
    return m_results.empty();
}

std::map<std::string, var> uva::database::active_record_relation::operator[](const size_t& index)
{
    std::map<std::string, var> result;

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

std::string sql_buffer;

std::string uva::database::active_record_relation::commit_sql() const
{
    var arr = empty_array;

    sql_buffer.clear();
    sql_buffer.reserve(query_buffer_lenght); 

    if(m_update.size()) {
        sql_buffer += "UPDATE " + m_table->m_name + " SET ";

        sql_buffer += uva::string::join(
            uva::string::join(m_update, [](const auto& val) {
                std::string val_str = val.second.to_s();
                if(val.second.type == var::var_type::string) {
                    val_str = uva::string::prefix_sufix(val_str, "'", "'");
                }
                return std::format("{}={}", val.first, val_str);
            }),
        ',');
    }

    if(m_select.size() && !m_update.size()) {
        sql_buffer += "SELECT " + m_select;
    }

    if(m_from.size() && !m_update.size()) {
        sql_buffer += " FROM " + m_from;
    }

    if(m_where.size()) {
        sql_buffer += " WHERE " + m_where;
    }

    if(!m_unscoped) {
        sql_buffer += (m_where.size() ? " AND " : " WHERE ") + (std::string)"removed = 0";
    }

    if(m_group.size()) {
        sql_buffer += " GROUP BY " + m_group;
    }

    if(m_order.size()) {
        sql_buffer += " ORDER BY " + m_order;
    }

    if(m_limit.size()) {
        sql_buffer += " LIMIT " + m_limit;
    }

    if(m_insert.size()) {
        sql_buffer += " INSERT INTO ";
        sql_buffer += m_into || m_from || m_table->m_name;

        if(m_columns.size())
        {
            sql_buffer += "(";

            sql_buffer += uva::string::join(m_columns, ',');

            sql_buffer += ")";
        }

        sql_buffer += " VALUES ";

        for(size_t i = 0; i < m_insert.size(); ++i)
        {
            sql_buffer.push_back('(');

            for(size_t x = 0; x < m_insert[i].size(); ++x)
            {
                if(m_insert[i][x].type == var::var_type::string)
                {
                    sql_buffer.push_back('\'');
                        m_insert[i][x].each([](const char& c) {
                            sql_buffer.push_back(c);
                            if(c == '\'')
                            {
                                sql_buffer.push_back(c);
                            }
                        });
                    sql_buffer.push_back('\'');
                } else {
                    sql_buffer += m_insert[i][x].to_s();
                }

                if(x < m_insert[i].size()-1)
                {
                    sql_buffer.push_back(',');
                }
            }

            sql_buffer.push_back(')');

            if(i < m_insert.size()-1)
            {
                sql_buffer.push_back(',');
            }
        }
    }

    if(m_returning.size())
    {
        sql_buffer += " RETURNING ";
        sql_buffer += m_returning;
    }

    sql_buffer += ";";

#ifndef _NDEBUG
    if(sql_buffer.size() > query_buffer_lenght)
    {
        uva::console::log_warning("Query string is bigger than buffer lenght ({} vs {}). Consider to increase query_buffer_lenght to get better performance.", sql_buffer.size(), query_buffer_lenght);
    }
#endif

    return sql_buffer;
}

void uva::database::active_record_relation::commit()
{
    std::string sql = commit_sql();

    commit(sql);
}

void uva::database::active_record_relation::commit_without_prepare()
{
    std::string sql = commit_sql();

    commit_without_prepare(sql);
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

    sqlite3_connection* connection = (sqlite3_connection*)uva::database::basic_connection::get_connection();

    char* error_msg = nullptr;
    struct callback_data {
        bool firstCalback = true;
        uva::database::active_record_relation* me;
    };

    callback_data data;
    data.me = this;

    int error = 0;
    auto elapsed = uva::diagnostics::measure_function([&]
    {
        error = sqlite3_exec(connection->get_database(), sql.c_str(), [](void *pdata, int columnCount, char **columnValue, char **columnNames) {

        callback_data* data = (callback_data*)pdata;
        std::vector<var> cols;

        for(size_t i = 0; i < columnCount; ++i) {

            if(data->firstCalback) {
                std::string colName = columnNames[i];
                
                data->me->m_columnsNames.push_back(colName);
                data->me->m_columnsIndexes.insert({colName, i});

                data->me->m_columnsTypes.push_back(var::var_type::string);
            }
        
            data->firstCalback = false;
            cols.push_back(columnValue[i]);
        }

        data->me->m_results.push_back(cols);

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

    if(enable_query_cout_printing)
    {
        #ifdef USE_FMT_FORMT
            std::string result = std::format("({} ms) {}", std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), sql.size() > 1000 ? sql.substr(0, 1000) : sql);
        #else
            std::string result = std::format("({}) {}", std::chrono::duration_cast<std::chrono::milliseconds>(elapsed), sql.size() > 1000 ? sql.substr(0, 1000) : sql);
        #endif

        std::cout << uva::console::color(color_code) << result << std::endl;
    }

    if(!error_report.empty()) {
        if(enable_query_cout_printing) {
            std::cout << uva::console::color(uva::console::color_code::red) << error_report << std::endl;
        }
        throw std::runtime_error(error_report);
    }
}

uva::database::active_record_relation::operator uva::core::var()
{
    std::vector<var> values;

    uva::database::active_record_relation rel = *this;
    rel.commit();

    for(auto& value : rel.m_results)
    {
        int col_it = 0;
        std::map<var, var> row;
        for(const std::string& col : rel.m_columnsNames) {
            row.insert({col,std::move(value[col_it])});
            col_it++;
        }
        values.push_back(std::move(row));
    }

    return var(std::move(values));
}

void uva::database::active_record_relation::commit(const std::string& sql)
{
    m_columnsNames.clear();
    m_columnsIndexes.clear();

    std::string error_report;

    sqlite3_stmt* stmt;
    int error = 0;
    char* error_msg = nullptr;

    auto elapsed = uva::diagnostics::measure_function([&] {

        sqlite3_connection* connection = (sqlite3_connection*)uva::database::basic_connection::get_connection();

        error = sqlite3_prepare_v2(connection->get_database(), sql.c_str(), (int)sql.size(), &stmt, nullptr);


        if(error) {
            error_msg = (char*)sqlite3_errmsg(connection->get_database());
        }
 
        if(error) {
            error_report = error_msg;
            return;
        }

        std::vector<var::var_type> columns_types;

        size_t colCount = sqlite3_column_count(stmt);

        for (int colIndex = 0; colIndex < colCount; colIndex++) {        

            std::string name = sqlite3_column_name(stmt, colIndex);
            m_columnsNames.push_back(name);

            const char* type_c_str = sqlite3_column_decltype(stmt, colIndex);

            //Assume text for any unknown type
            if(!type_c_str) {
                type_c_str = "TEXT";
            }

            std::string type_str = type_c_str;

            columns_types.push_back(sql_delctype_to_value_type(type_str));
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {

            std::vector<var> cols;

            for (int colIndex = 0; colIndex < colCount; colIndex++) {

                var holder;

                size_t type = sqlite3_column_type(stmt, colIndex);
                var::var_type& value_type = columns_types[colIndex];

                if(type == SQLITE_NULL)
                {
                    holder = null;
                }
                else
                {
                    switch (value_type)
                    {
                        case var::var_type::integer:
                        {
                            int64_t value = sqlite3_column_int64(stmt, colIndex);
                            holder = value;

                            break;
                        }
                        case var::var_type::real:
                        {
                            double value = sqlite3_column_double(stmt, colIndex);
                            holder = value;

                            break;
                        }
                        case var::var_type::string:
                        {
                            const unsigned char* value = sqlite3_column_text(stmt, colIndex);

                            if(!value) {
                                throw std::runtime_error("attempting to read null string");
                            }

                            holder = value;

                            break;
                        }
                        default:
                            throw std::runtime_error("reading SQL results invalid data type");
                        break;
                    }

                }

                cols.push_back(std::move(holder));
                #ifdef UVA_DEBUG_LEVEL > 1
                    if(cols.back().type != value_type)
                    {
                        throw std::runtime_error("reading SQL results wrong data type");
                    }
                #endif
            }

            m_results.push_back(std::move(cols));
        }
    });

    error = sqlite3_finalize(stmt);

    if (error) {
        //std::string error_report = error_msg;
        //sqlite3_free(error_msg);
        //throw std::runtime_error(""/*error_report*/);
    }    

    uva::console::color_code color_code;

    if(error_report.empty()) {
        color_code = uva::console::color_code::blue;
    } else {
        color_code = uva::console::color_code::red;
    }

    if(enable_query_cout_printing)
    {
        #ifdef USE_FMT_FORMT
            std::string result = std::format("({} ms) {}", std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), sql.size() > 1000 ? sql.substr(0, 1000) : sql);
        #else
            std::string result = std::format("({}) {}", std::chrono::duration_cast<std::chrono::milliseconds>(elapsed), sql.size() > 1000 ? sql.substr(0, 1000) : sql);
        #endif

        std::cout << uva::console::color(color_code) << result << std::endl;
    }

    if(!error_report.empty()) {
        if(enable_query_cout_printing)
        {
            std::cout << uva::console::color(uva::console::color_code::red) << error_report << std::endl;
        }
        throw std::runtime_error(error_report);
    }
}

//ACTIVE RECORD RELATION

//BASIC MIGRATION

uva::database::basic_migration::basic_migration(const std::string& __title, std::string_view __filename)
    : title(__title), uva::database::basic_active_record(0)
{
    const char* migrations_last_dir = "migrations/";
    static const size_t migrations_last_dir_len = strlen(migrations_last_dir);
    const char* __filename_str_ptr = __filename.substr(__filename.find(migrations_last_dir)+migrations_last_dir_len).data();

    while(*__filename_str_ptr && isdigit(*__filename_str_ptr)) {
        date_str.push_back(*__filename_str_ptr);
        ++__filename_str_ptr;
    }

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
    } catch(const char* e)
    {
        std::cout << uva::console::color(uva::console::color_code::red) << std::format("{}: {}\n\nAborting...", this->title, e) << std::endl;
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

    std::sort(migrations.begin(), migrations.end(), [](const uva::database::basic_migration* lhs, const uva::database::basic_migration* rhs){
        return lhs->date_str < rhs->date_str;
    });

    for(uva::database::basic_migration* migration : migrations)
    {
        if(migration->is_pending()) {
            migration->call_change();
        }
    }
}

void uva::database::basic_migration::apply()
{
    at("title") = title;
    save();
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
    uva::database::active_record_relation().commit_without_prepare(std::format("CREATE INDEX IF NOT EXISTS idx_{}_on_{} ON {}({});", table_name, uva::string::replace(column, ',', '_'), table_name, column));
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