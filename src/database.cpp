#include <database.hpp>
#include <time.hpp>

// STATIC MEMBERS

std::map<std::string, var::var_type> uva::db::sql_values_types_map 
{
    { "NULL", var::var_type::null_type },
    { "TEXT", var::var_type::string },
    { "INTEGER", var::var_type::integer },
    { "REAL", var::var_type::real },
};

var::var_type uva::db::sql_delctype_to_value_type(const std::string& type)
{
    auto it = sql_values_types_map.find(type);

    if(it == sql_values_types_map.end()) 
    {
        if(type.starts_with("VARCHAR")) {
            return var::var_type::string;
        }
    }
    else
    {
        return it->second;
    }

    throw std::runtime_error(std::format("failed to convert from '{}' to value_type", type));
}


void uva::db::within_transaction(std::function<void()> __f)
{
    // uva::db::basic_connection::get_connection()->begin_transaction();
    // try {
    //     __f();
    // } catch(std::exception e)
    // {
    //     uva::db::basic_connection::get_connection()->end_transaction();    
    //     throw;
    // }
    // uva::db::basic_connection::get_connection()->end_transaction();
}


size_t uva::db::query_buffer_lenght = uva::db::query_buffer_lenght_default;
bool   uva::db::enable_query_cout_printing = uva::db::enable_query_cout_printing_default;

// END STATIC MEMBERS

using basic_migration = uva::db::basic_migration;
uva_db_define_full(basic_migration, "database_migrations");

//BASIC CONNECTION

uva::db::basic_connection* uva::db::basic_connection::s_connection = nullptr;

uva::db::basic_connection::basic_connection()
{
    s_connection = this;
}

uva::db::basic_connection* uva::db::basic_connection::get_connection()
{
    return s_connection;
}

//SQLITE3 CONNECTION

uva::db::sqlite3_connection::sqlite3_connection(const std::filesystem::path& database_path)
    : m_database_path(database_path)
{
    open();
}

uva::db::sqlite3_connection::~sqlite3_connection()
{
    sqlite3_close(m_database);
}

bool uva::db::sqlite3_connection::open()
{
    std::filesystem::path folder = m_database_path.parent_path();
    if (!std::filesystem::exists(folder)) {
        if (!std::filesystem::create_directories(folder)) {
            throw std::runtime_error("unknow error while creating database directories.");
        }
    }
    int error = sqlite3_open(m_database_path.string().c_str(), &m_database);    
    if (error) {        
        throw std::runtime_error("unknow error while opening database.");
    }
    return m_database;
}

bool uva::db::sqlite3_connection::open(const std::filesystem::path& path)
{
    m_database_path = path;
    int error = sqlite3_open(m_database_path.string().c_str(), &m_database);
    if (error) {
        throw std::runtime_error("unknow error while opening database.");
    }
    return m_database;
}


bool uva::db::sqlite3_connection::is_open() const
{
    return m_database;
}

//END SQLITE3 CONNECTION

//TABLE

uva::db::table::table(const std::string& name, const std::map<std::string, std::string>& cols)
    : m_name(name), m_columns(cols)
{
    uva::db::table::add_table(this);
}

uva::db::table::table(const std::string& name)
    : m_name(name)
{
    uva::db::table::add_table(this);
}

std::map<std::string, uva::db::table*>& uva::db::table::get_tables()
{
    static std::map<std::string, table*> s_tables;
    return s_tables;
}

void uva::db::table::add_table(uva::db::table* table)
{
    auto tables = get_tables();
    tables.insert({table->m_name, table});
}

std::string sql;
void uva::db::table::create_table(uva::db::table* table)
{
    /* Create SQL statement */
    sql.reserve(1024);
    
    sql = "CREATE TABLE IF NOT EXISTS " + table->m_name + "(";

    for(const auto& col : table->m_columns)
    {
        sql += col.first + " " + col.second + ", ";
    }

    sql.pop_back();
    sql.pop_back();

    sql += " );";

    uva::db::basic_connection::get_connection()->run_sql(sql);
}

uva::db::table* uva::db::table::get_table(const std::string& name) {

    auto tables = get_tables();

    auto it = tables.find(name);

    if (it == tables.end()) {

        if(name == "database_migrations") {
            uva::db::table * info_table = new uva::db::table("database_migrations",
            {
                { "id",         "SERIAL PRIMARY KEY" },
                { "title",      "TEXT NOT NULL" },
                { "updated_at", "TIMESTAMP NOT NULL" },
                { "created_at", "TIMESTAMP NOT NULL" },
                { "removed",    "BOOLEAN NOT NULL DEFAULT false" },    
            });

            //The table is only created if not exists
            uva::db::table::create_table(info_table);

            return info_table;
        } else {
            return new uva::db::table(name);
        }

        //throw
        return nullptr;
    }

    return it->second;
}

//END TABLE

//ACTIVE RECORD

uva::db::basic_active_record::basic_active_record()
{

}

uva::db::basic_active_record::basic_active_record(const basic_active_record& _record)
    : values(_record.values)
{
}

uva::db::basic_active_record::basic_active_record(basic_active_record&& _record)
    : values(std::move(_record.values))
{
}

uva::db::basic_active_record::basic_active_record(var &&__values)
{
    if(__values.is_a<var::map>()) {
        for(auto& pair : __values.as<var::map>())
        {
            const std::string& key = pair.first.as<var::string>();
            const var& value = pair.second;

            values[std::move(const_cast<std::string&>(key))] = std::move(const_cast<var&>(value));
        }

        __values.clear();
    }
}

uva::db::basic_active_record::basic_active_record(const var &__values)
{
    for(const auto& value : __values.as<var::map>())
    {
        values[value.first.to_s()] = value.second;
    }
}

bool uva::db::basic_active_record::present() const {
    if (values.empty()) {
        return false;
    }

    auto it = values.find("id");

    if(it == values.end()) {
        return false;
    }

    return it->second;
}

void uva::db::basic_active_record::destroy() {

}

void uva::db::basic_active_record::call_before_create()
{
    values["created_at"] = uva::time::iso_now();
    call_before_save();

    before_create();
}

void uva::db::basic_active_record::call_before_save()
{
    values["updated_at"] = uva::time::iso_now();

    before_save();
}

void uva::db::basic_active_record::call_before_update()
{
    call_before_save();

    before_update();
}

uva::db::basic_active_record& uva::db::basic_active_record::operator=(const uva::db::basic_active_record& other)
{
    values = other.values;
    return *this;
}

var& uva::db::basic_active_record::at(const std::string& str)
{
    var& value = values[str];
    return value;
}

const var& uva::db::basic_active_record::at(const std::string& str) const
{
    auto it = values.find(str);

    if(it == values.end()) {
        throw std::runtime_error(std::format("Record from {} has no column named {}", get_table()->m_name, str));
    }

    return it->second;
}

var& uva::db::basic_active_record::operator[](const std::string& str) {
    return at(str);
}

const var& uva::db::basic_active_record::operator[](const std::string& str) const {
    return at(str);
}

var& uva::db::basic_active_record::operator[](const char* str)
{
    return at(std::string(str));
}

const var& uva::db::basic_active_record::operator[](const char* str) const
{
    return at(std::string(str));
}

std::string uva::db::basic_active_record::to_s() const
{
    std::string joined_values = uva::string::join(uva::string::join(values, [](const std::pair<std::string, var> & v){
        return std::format("{}={}", v.first, v.second.to_s());
    }), ", ");

    return std::format("<{}Class> {{ {} }}", class_name(), joined_values);
}

void uva::db::basic_active_record::save()
{   
    auto it = values.find("id"); 

    if(it == values.end() || it->second.is_null() || it->second.type == var::var_type::undefined) {
        call_before_create();
        uva::db::active_record_relation(get_table()).create(values);
    } else {
        call_before_update();
        uva::db::active_record_relation(get_table()).update(values);
    }
}

void uva::db::basic_active_record::update(const std::string& col, const var& value)
{
    // values[col] = value;
    // before_update();

    // uva::db::table* table = get_table();
    // table->update(values["id"], col, value);

    // before_save();
}

void uva::db::basic_active_record::update(const std::map<std::string, var>& _values)
{
    // for(const auto& value : _values) {
    //     values[value.first] = value.second;
    // }

    // before_update();
    
    // get_table()->update(values["id"], _values);

    // before_save();
}

//END ACTIVE RECORD

//MIGRATION


//END MIGRATION

//ACTIVE RECORD RELATION

uva::db::active_record_relation::active_record_relation(uva::db::table* table)
    : m_table(table)
{
    
}

uva::db::active_record_relation& uva::db::active_record_relation::select(const std::string& select)
{
    if(m_select.size() && m_select != "*") {
        m_select.push_back(',');
        m_select.push_back(' ');

        m_select += select;
    }  else {
        m_select = select;
    }

    return *this;
}

uva::db::active_record_relation& uva::db::active_record_relation::from(const std::string& from)
{
    m_from = from;

    return *this;
}

uva::db::active_record_relation &uva::db::active_record_relation::group_by(const std::string &group)
{
    m_group = group;
    return *this;
}

uva::db::active_record_relation& uva::db::active_record_relation::order_by(const std::string& order)
{
    m_order = order;

    return *this;
}

uva::db::active_record_relation& uva::db::active_record_relation::limit(const std::string& limit)
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

uva::db::active_record_relation& uva::db::active_record_relation::limit(const size_t& limit)
{
    m_limit = std::to_string(limit);
    return *this;
}

uva::db::active_record_relation& uva::db::active_record_relation::insert(var& insert)
{
    m_insert = std::move(insert);
    return *this;
}

uva::db::active_record_relation& uva::db::active_record_relation::insert(std::vector<var>& insert)
{
    var v(std::move(insert));

    m_insert.push_back(std::move(v));
    return *this;
}

uva::db::active_record_relation& uva::db::active_record_relation::insert(std::vector<std::vector<var>>& __insert)
{
    m_insert.reserve(__insert.size());
    for(auto& insert : __insert)
    {
        var v = std::move(insert);

        m_insert.push_back(std::move(v));
    }
    return *this;
}

uva::db::active_record_relation& uva::db::active_record_relation::columns(const std::vector<std::string>& columns)
{
    m_columns = columns;
    return *this;
}

uva::db::active_record_relation& uva::db::active_record_relation::into(const std::string& into)
{
    m_into = into;
    return *this;
}

uva::db::active_record_relation& uva::db::active_record_relation::returning(const std::string& returning)
{
    m_returning = returning;
    return *this;
}

std::vector<var> uva::db::active_record_relation::pluck(const std::string& col)
{
    std::vector<var> values;

    uva::db::active_record_relation rel = *this;

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

std::vector<std::vector<var>> uva::db::active_record_relation::pluckm(const std::string& cols)
{
    std::vector<std::vector<var>> values;

    uva::db::active_record_relation rel = *this;

    rel.select(cols);

    rel.commit();

    return rel.m_results;
}

size_t uva::db::active_record_relation::count(const std::string& count) const
{
    uva::db::active_record_relation count_relation = *this;

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

    } catch(const std::exception& e) {
        (void)e;
    }

    return 0;
}

var uva::db::active_record_relation::first()
{
    uva::db::active_record_relation first_relation = *this;

    if(!first_relation.m_order.size()) {
        first_relation = first_relation.order_by("id");
    }

    first_relation = first_relation.limit(1);

    var results = uva::db::basic_connection::get_connection()->select_all(first_relation.to_sql());

    if(!results.size()) {
        return null;
    }

    return results[0];
}

void uva::db::active_record_relation::each_with_index(std::function<void(std::map<std::string, var>& value, const size_t&)> func)
{
    // size_t index = 0;
    // size_t last_id = 0;

    while(true) {
        // uva::db::active_record_relation copy = uva::db::active_record_relation(*this);

        // if(index) {
        //     copy.where("{} > {}", m_order || "id", last_id).first();
        // }

        // auto first = copy.first();

        // if(first.empty()) {
        //     return;
        // } else {
        //     last_id = first[m_order || "id"];

        //     func(first, index);
        // }

        // ++index;
    }
}

void uva::db::active_record_relation::each(std::function<void(std::map<std::string, var>&)> func)
{
    each_with_index([&](std::map<std::string, var>& value, const size_t& index){
        func(value);
    });
}

var uva::db::active_record_relation::create(var values)
{
    if(values.is_a<var::map>()) {
        auto& map = values.as<var::map>();

        m_insert = var::array();
        std::vector<var> values;

        for(auto& pair : map) {
            if(pair.second.type != var::var_type::undefined) {
                const std::string& key = pair.first.as<var::string>();
                m_columns.push_back(key);
                values.push_back(pair.second);
            }
        }

        m_insert.push_back(values);
    }

    std::string sql = commit_sql();
    var rows = uva::db::basic_connection::get_connection()->select_all(sql);

    if(rows.size() == 1) {
        return rows[0];
    }

    return rows;
}

var uva::db::active_record_relation::update(var values)
{
    if(values.is_a<var::map>()) {
        auto& map = values.as<var::map>();

        for(auto& pair : map) {
            m_update[pair.first.as<var::string>()] = pair.second;
        }
    }

    std::string sql = commit_sql();
    values = uva::db::basic_connection::get_connection()->select_all(sql);

    return values;
}

bool uva::db::active_record_relation::empty()
{
    //commit();
    return m_results.empty();
}

uva::db::active_record_relation uva::db::active_record_relation::unscoped()
{
    uva::db::active_record_relation unscoped = *this;

    unscoped.m_unscoped = true;

    return unscoped;
}

void uva::db::active_record_relation::append_where(const std::string& where)
{
    //TODO: Remove previous columns values in m_where

    if(m_where.size()) {
        m_where += "AND " + where;
    } else {
        m_where = where;
    }
}

std::string sql_buffer;

std::string uva::db::active_record_relation::commit_sql() const
{
    var arr = var::array();

    sql_buffer.clear();
    sql_buffer.reserve(query_buffer_lenght); 

    if(m_update.size()) {
        sql_buffer += "UPDATE " + m_table->m_name + " SET ";

        size_t i = 0;
        for(const auto& value : m_update)
        {
            if(value.second.type != var::var_type::undefined) {
                sql_buffer.append(value.first);

                sql_buffer.append(" = ");

                value.second.append_to(sql_buffer);

                if(i >= m_update.size() - 1) {
                    break;
                }

                sql_buffer.append(", ");
            }

            ++i;
        }
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

    if(!m_unscoped && !m_insert.size()) {
        static std::string defualt_scope = "removed = false";
        sql_buffer += (m_where.size() ? " AND " : " WHERE ") + defualt_scope;
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
                var value = m_insert[i][x];
                if(value.type == var::var_type::null_type) {
                    sql_buffer += "null";
                }
                else if(value.is_a<var::string>())
                {
                    sql_buffer.push_back('\'');
                        value.each([](const char& c) {
                            sql_buffer.push_back(c);
                            if(c == '\'')
                            {
                                sql_buffer.push_back(c);
                            }
                        });
                    sql_buffer.push_back('\'');
                } else if(value.is_a<var::string>()) {
                    sql_buffer += value.to_s();
                } else {
                    sql_buffer.push_back('\'');
                    sql_buffer += value.to_typed_s('{', '}', true);
                    sql_buffer.push_back('\'');
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

void uva::db::active_record_relation::commit()
{
    std::string sql = commit_sql();

    commit(sql);
}

void uva::db::active_record_relation::commit_without_prepare()
{
    std::string sql = commit_sql();

    commit_without_prepare(sql);
}

std::string uva::db::active_record_relation::to_sql() const
{
    std::string sql = commit_sql();

    return sql;
}

void uva::db::active_record_relation::commit_without_prepare(const std::string& sql)
{
    m_columnsNames.clear();
    m_columnsIndexes.clear();

    sqlite3_connection* connection = (sqlite3_connection*)uva::db::basic_connection::get_connection();

    char* error_msg = nullptr;
    struct callback_data {
        bool firstCalback = true;
        uva::db::active_record_relation* me;
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

uva::db::active_record_relation::operator uva::core::var()
{
    std::string sql = commit_sql();
    return uva::db::basic_connection::get_connection()->select_all(sql);
}

void uva::db::active_record_relation::commit(const std::string& sql)
{
    m_columnsNames.clear();
    m_columnsIndexes.clear();

    std::string error_report;

    sqlite3_stmt* stmt;
    int error = 0;
    char* error_msg = nullptr;

    auto elapsed = uva::diagnostics::measure_function([&] {

        sqlite3_connection* connection = (sqlite3_connection*)uva::db::basic_connection::get_connection();

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
                #if UVA_DEBUG_LEVEL > 1
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

uva::db::basic_migration::basic_migration(const std::string& __title, std::string_view __filename)
    : title(__title)
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

 std::string uva::db::basic_migration::make_label()
 {
    time_t date = at("created_at");

    tm* local_time = std::localtime(&date);

    char buffer[100];

    std::strftime(buffer, 100, "%d%m%Y%H%M%S", local_time);

    std::string date_str = buffer;

    return date_str + "_" + title;
}

bool uva::db::basic_migration::is_pending() {
    var migration = all().where("title = {}", title).select("1 AS exists").first();

    return migration.is_null();
}

// uva::db::table* uva::db::basic_migration::get_table()
// {
//     static uva::db::table* info_table = nullptr;
    
//     if(!info_table) {
//         info_table = new uva::db::table("database_migrations",
//         {
//             { "title", "TEXT" },
//             { "date",  "INTEGER"}
//         });

//         //The table is only created if not exists
//         uva::db::basic_connection::get_connection()->create_table(info_table);
//     }

//     return info_table;
// }

void uva::db::basic_migration::call_change()
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

    } catch(std::exception& e)
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

std::vector<uva::db::basic_migration*>& uva::db::basic_migration::get_migrations()
{
    static std::vector<uva::db::basic_migration*> s_migrations;

    return s_migrations;
}

void uva::db::basic_migration::do_pending_migrations()
{
    std::vector<uva::db::basic_migration*>& migrations = uva::db::basic_migration::get_migrations();

    std::sort(migrations.begin(), migrations.end(), [](const uva::db::basic_migration* lhs, const uva::db::basic_migration* rhs){
        return lhs->date_str < rhs->date_str;
    });

    for(uva::db::basic_migration* migration : migrations)
    {
        if(migration->is_pending()) {
            migration->call_change();
        }
    }
}

void uva::db::basic_migration::apply()
{
    at("title") = title;
    save();
}

void uva::db::basic_migration::add_table(const std::string& table_name, const std::map<std::string, std::string>& cols)
{
    uva::db::table* new_table = new uva::db::table(table_name, cols);
    uva::db::table::create_table(new_table);
}

void uva::db::basic_migration::drop_table(const std::string& table_name)
{
    uva::db::active_record_relation().commit_without_prepare(std::format("DROP TABLE {};", table_name));
}

void uva::db::basic_migration::add_index(const std::string& table_name, const std::string& column)
{
    uva::db::basic_connection::get_connection()->run_sql(std::format("CREATE INDEX IF NOT EXISTS idx_{}_on_{} ON {}({});", table_name, uva::string::replace(column, ',', '_'), table_name, column));
}

void uva::db::basic_migration::drop_column(const std::string& table_name, const std::string& name)
{
    uva::db::table* table = uva::db::table::get_table(table_name);
   // table->drop_column(name);
}


void uva::db::basic_migration::add_column(const std::string& table_name, const std::string& name, const std::string& type, const std::string& default_value) const
{
    uva::db::table* table = uva::db::table::get_table(table_name);
  //  table->add_column(name, type, default_value);
}

void uva::db::basic_migration::change_column(const std::string& table_name, const std::string& name, const std::string& type) const
{
    uva::db::table* table = uva::db::table::get_table(table_name);
  //  table->change_column(name, type);
}

//END BASIC MIGRATION