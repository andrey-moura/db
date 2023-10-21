#include <database.hpp>

uva::database::postgres_connection::postgres_connection(var params)
{
    std::string cmd;
    for(auto& param : params.as<var::map>()) {
        cmd += param.first.to_s() + " = " + param.second.to_s() + " ";
    }

    m_pqxx_connection = std::make_shared<pqxx::connection>(cmd);

    bool is_open = m_pqxx_connection->is_open();
}

uva::database::postgres_connection::~postgres_connection()
{
}

void uva::database::postgres_connection::run_sql(const std::string &sql)
{
    std::string error_msg;

    auto elapsed = uva::diagnostics::measure_function([&]
    {
        try
        {
            // Start a transaction.  In libpqxx, you always work in one.
            pqxx::work w(*m_pqxx_connection);

            // work::exec1() executes a query returning a single row of data.
            // We'll just ask the database to return the number 1 to us.
            pqxx::result r = w.exec0(sql);

            // Commit your transaction.  If an exception occurred before this
            // point, execution will have left the block, and the transaction will
            // have been destroyed along the way.  In that case, the failed
            // transaction would implicitly abort instead of getting to this point.
            w.commit();

            // Look at the first and only field in the row, parse it as an integer,
            // and print it.
        }
        catch (const std::exception &e)
        {
            error_msg = e.what();
        }
    });

    uva::console::color_code color_code;

    if(error_msg.size()) {
        color_code = uva::console::color_code::red;

        if(enable_query_cout_printing) {
            std::cout << uva::console::color(color_code) << error_msg << std::endl;
        }
    } else {
        color_code = uva::console::color_code::blue;
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

    if(error_msg.size()) {
        throw std::runtime_error("error");
    }
}

var uva::database::postgres_connection::select_all(const std::string &sql)
{
    std::vector<var> rows;
    std::string error_msg;

    std::string pqxx_sql;

    auto elapsed = uva::diagnostics::measure_function([&]
    {
        try
        {
            // Start a transaction.  In libpqxx, you always work in one.
            pqxx::work w(*m_pqxx_connection);

            // work::exec1() executes a query returning a single row of data.
            // We'll just ask the database to return the number 1 to us.
            pqxx::result results = w.exec(sql);

            // Commit your transaction.  If an exception occurred before this
            // point, execution will have left the block, and the transaction will
            // have been destroyed along the way.  In that case, the failed
            // transaction would implicitly abort instead of getting to this point.
            w.commit();

            //select typname, oid from pg_type;

            enum pqxx_types
            {
                boolean = 16,
                int8 = 20,
                int2 = 21,
                int4 = 23,
                text = 25,
                timestamp = 1114,
                varchar = 1043,
            };

            std::map<var, var> row;
            for(size_t row_index = 0; row_index < results.size(); ++row_index)
            {
                for(size_t col_index = 0; col_index < results.columns(); ++col_index)
                {
                    pqxx::oid col_type = results.column_type((int)col_index);
                    const char* col_name = results.column_name(col_index);

                    if(results[row_index][(int)col_index].is_null()) {
                        row[col_name] = null;
                        continue;
                    }

                    switch (col_type)
                    {
                    case pqxx_types::boolean:
                        row[col_name] = results[row_index][(int)col_index].as<bool>();
                    break;
                    case pqxx_types::int8:
                    case pqxx_types::int2:
                    case pqxx_types::int4:
                        row[col_name] = results[row_index][(int)col_index].as<int>();
                    break;
                    case pqxx_types::text:
                    case pqxx_types::varchar:
                        row[col_name] = results[row_index][(int)col_index].as<const char*>();
                    break;
                    case pqxx_types::timestamp:
                        row[col_name] = results[row_index][(int)col_index].as<const char*>();
                    break;
                    default:
                        break;
                    }
                }

                rows.push_back(std::move(row));
            }
        }
        catch (const pqxx::data_exception &e)
        {
            pqxx_sql = e.query();
            error_msg = e.what();
        }
        catch (const std::exception &e)
        {
            error_msg = e.what();
        }
    });

    uva::console::color_code color_code;

    if(error_msg.size()) {
        color_code = uva::console::color_code::red;

        if(enable_query_cout_printing) {
            std::cout << uva::console::color(color_code) << error_msg << std::endl;
        }
    } else {
        color_code = uva::console::color_code::blue;
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

    if(error_msg.size()) {
        throw std::runtime_error("error");
    }

    return var(std::move(rows));
}