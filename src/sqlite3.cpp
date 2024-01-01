#include <uva/db.hpp>

void uva::db::sqlite3_connection::run_sql(const std::string &sql)
{
    sqlite3_connection* connection = (sqlite3_connection*)uva::db::basic_connection::get_connection();
    char* error_msg = nullptr;
    int error = 0;
    
    auto elapsed = uva::diagnostics::measure_function([&]
    {
        error = sqlite3_exec(connection->get_database(), sql.c_str(), nullptr, nullptr, &error_msg);
    });

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

var uva::db::sqlite3_connection::select_all(const std::string &sql)
{
    std::string error_report;

    sqlite3_stmt* stmt;
    int error = 0;
    char* error_msg = nullptr;

    std::vector<var> rows;

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
        std::vector<std::string> column_names;

        size_t col_count = sqlite3_column_count(stmt);

        for (int colIndex = 0; colIndex < col_count; colIndex++) {        

            std::string name = sqlite3_column_name(stmt, colIndex);
            column_names.push_back(name);

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

            for (int colIndex = 0; colIndex < col_count; colIndex++) {

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

            rows.push_back(std::move(cols));
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

    return var(std::move(rows));
}