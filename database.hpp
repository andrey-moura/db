#pragma once

#include <string>
#include <filesystem>
#include <map>
#include <map>
#include <exception>
#include <vector>
#include <iterator>
#include <functional>

#include "sqlite3.h"

#include <console.hpp>
#include <diagnostics.hpp>

#define uva_database_params(...) __VA_ARGS__ 

#define uva_database_declare(record) \
public:\
    record() : uva::database::basic_active_record(std::string::npos) { } \
    record(size_t id) : uva::database::basic_active_record(id) { } \
    const uva::database::table* get_table() const override { return table(); } \
    uva::database::table* get_table() override { return table(); } \
    static uva::database::table* table(); \
    static size_t create() { return table()->create(); } \
    static size_t create(const std::map<std::string, std::string>& relations) { return table()->create(relations); } \
    static void create(std::vector<std::map<std::string, std::string>>& relations) { table()->create(relations); } \
    static size_t column_count() { return table()->m_columns.size(); } \
    static std::vector<std::pair<std::string, std::string>>& columns() { return table()->m_columns; } \
    static uva::database::active_record_relation all() { return uva::database::active_record_relation(table()).select("*").from(table()->m_name).where("removed=0");  } \
    static uva::database::active_record_relation where(const std::string& where) { return record::all().where(where); } \
    static uva::database::active_record_relation from(const std::string& from) { return record::all().from(from); } \
    static uva::database::active_record_relation select(const std::string& select) { return record::all().select(select); } \
    static size_t count(const std::string& count = "*") { return record::all().count(); } \

#define uva_database_define_sqlite3(record, rows, db) \
uva::database::table* record::table() { \
\
    uva::database::sqlite3_connection* connection = uva::database::sqlite3_connection::connect(db);\
    std::string table_name = #record;\
    for(char& c : table_name) { if(isalpha(c)) { c = tolower(c); } }\
\
    static uva::database::table* table = new uva::database::table(table_name + "s", rows, connection); \
    static uva::database::table* infotable = new uva::database::table(table_name + "s" + "_table_migrations", { \
        { "title", "TEXT" },\
        { "summary", "TEXT" },\
        { "date", "INTEGER" }\
    }, connection); \
    return table; \
}

#define uva_database_declare_migration(record) using record##Migration = uva::database::basic_migration<record>

namespace uva
{
    namespace database
    {        
        class table;
        class basic_active_record;

        class basic_connection
        {                    
        public:
            virtual bool open() = 0;
            virtual bool is_open() const = 0;
            virtual bool create_table(const table* table) const = 0;
            virtual void read_table(table* table) = 0;
            virtual bool insert(table* table, size_t id, const std::map<std::string, std::string>& relations) = 0;
            virtual bool insert(table* table, size_t id, const std::vector<std::map<std::string, std::string>>& relations) = 0;
            virtual void update(size_t id, const std::string& key, const std::string& value, table* table) = 0;
            virtual void destroy(size_t id, uva::database::table* table) = 0;
            virtual void add_column(uva::database::table* table, const std::string& name, const std::string& type, const std::string& default_value) = 0;
            virtual void change_column(uva::database::table* table, const std::string& name, const std::string& type) = 0;
        public:
            static std::map<std::string, basic_connection*>& get_connections();
            static basic_connection* get_connection(const std::string& connection);
            static void add_connection(const std::string& index, basic_connection* connection);
        };

        class sqlite3_connection : public basic_connection
        {
            public:
                sqlite3_connection();
                sqlite3_connection(const std::filesystem::path& database_path);
                ~sqlite3_connection();
            protected:
                sqlite3 *m_database = nullptr;
                std::filesystem::path m_database_path;
            public:
                sqlite3* get_database() const { return m_database; }
                static sqlite3_connection* connect(const std::filesystem::path& database);
                virtual bool open() override;
                virtual bool is_open() const override;
                bool open(const std::filesystem::path& path);
                virtual bool create_table(const table* table) const override;
                virtual void read_table(table* table) override;
                void alter_table(uva::database::table* table, const std::string& new_signature);
                virtual bool insert(table* table, size_t id, const std::map<std::string, std::string>& relations) override;
                virtual bool insert(table* table, size_t id, const std::vector<std::map<std::string, std::string>>& relations) override;
                virtual void update(size_t id, const std::string& key, const std::string& value, table* table) override;
                virtual void destroy(size_t id, uva::database::table* table) override;
                virtual void add_column(uva::database::table* table, const std::string& name, const std::string& type, const std::string& default_value) override;
                virtual void change_column(uva::database::table* table, const std::string& name, const std::string& type) override;
        };
 
        using result = std::vector<std::pair<std::string, std::string>>;
        using results = std::vector<std::vector<std::pair<std::string, std::string>>>;

        struct multiple_value_holder
        {
            enum class value_type {
                string,
                integer
            };

            value_type type;

            std::string str;
            int64_t integer;
        public:
            operator size_t()
            {
                return (size_t)integer;
            }
            multiple_value_holder& operator=(const char* c)
            {
                str = c;
                return *this;
            }
            multiple_value_holder& operator=(const unsigned char* c)
            {
                str = (const char*)c;
                return *this;
            }
            multiple_value_holder& operator=(const size_t& s)
            {
                integer = (int64_t)s;
                return *this;
            }
        };

        class active_record_relation
        {
        public:
            active_record_relation() = default;
            active_record_relation(table* table);
        private:
            std::string m_select;
            std::string m_from;
            std::string m_where;
            table* m_table;

            std::map<std::string, size_t> m_columnsIndexes;
            std::vector<std::string> m_columnsNames;
            std::vector<multiple_value_holder::value_type> m_columnsTypes;

            std::vector<std::vector<multiple_value_holder>> m_results;
        public:
            std::string to_sql() const;
        public:
            active_record_relation& select(const std::string& select);
            active_record_relation& from(const std::string& from);
            active_record_relation& where(const std::string& where);
            size_t count(const std::string& count = "*");
        private:
            std::string commit_sql() const;
            void commit();
            void commit(const std::string& sql);
        public:
        };

        class value 
        {
        public:
            value(const std::string& key, std::string& value, size_t id, table* table);
        private:
            std::string m_key;
            std::string& m_value;
            size_t m_recordId;
            table* m_table;
        public:
            bool present();
        public:
            operator std::string& ();
            operator size_t();
            friend std::string operator+(const std::string& other, uva::database::value& value);
            friend std::ostream& operator<<(std::ostream& out, uva::database::value& value)
            {
                out << (std::string)value;
                return out;
            }
            value& operator=(const std::string& other);
            value& operator=(const size_t& value);
            bool operator== (const std::string& other);
            bool operator!=(const std::string& other);
            bool operator== (const bool& other);
        };

        class table
        {
        public:
            table(const std::string& name,
                const std::vector<std::pair<std::string, std::string>>& columns,
                basic_connection* connection);

            std::string m_name;
            basic_connection* m_connection;
            std::vector<std::pair<std::string, std::string>> m_columns;
            std::map<size_t, std::map<std::string, std::string>> m_relations;
            size_t create();
            size_t create(const std::map<std::string, std::string>& relations);
            void create(std::vector<std::map<std::string, std::string>>& relations);
            size_t find(size_t id) const;
            size_t find_by(const std::map<std::string, std::string>& relations);
            size_t first();
            size_t last();
            void destroy(size_t id);
            bool relation_exists(size_t id) const;
            void update(size_t id, const std::string& key, const std::string& value);
            static std::map<std::string, table*>& get_tables();
            static table* get_table(const std::string& name);
            std::string& at(size_t id, const std::string& key);
            void add_column(const std::string& name, const std::string& type, const std::string& default_value);
            void change_column(const std::string& name, const std::string& type);
            std::vector<std::pair<std::string, std::string>>::const_iterator find_column(const std::string& col) const;
            std::vector<std::pair<std::string, std::string>>::iterator find_column(const std::string& col);
        };
        
        class basic_active_record
        {              
        public:
            basic_active_record(size_t _id);           
            basic_active_record(const basic_active_record& value);
            basic_active_record(value& _record);
        public:
            bool present() const;
            void destroy();            
        public:
            size_t id = -1; 
        protected:
            virtual const table* get_table() const = 0;
            virtual table* get_table() = 0;            
        public:
            value at(const std::string& key);
            value at(size_t index);
            value operator[](const std::string& key);
            value operator[](size_t index);

            // const value& operator[](const std::string& key) const {
            //     const table* table = get_table();
            //     auto recordIt = table->m_relations.find(id);
            //     if(recordIt == table->m_relations.end()) {
            //         throw std::out_of_range(table->m_name + " do not contains a record with id " + std::to_string(id));
            //     }

            //     auto relationIt = recordIt->second.find(key);

            //     if(relationIt == recordIt->second.end()) {
            //         throw std::out_of_range(table->m_name + " do not contains a column with name " + key);
            //     }

            //     return relationIt->second;
            // }
        };
        template<typename Record>
        class basic_migration
        {
            size_t id;
            time_t date = -1;
            bool m_pending = false;
            std::function<bool(const basic_migration<Record>&)> m_migrate = nullptr;
            std::string title;
            std::string summary;
        public:
            std::string label;
        public:
            basic_migration(const basic_migration<Record>& other) = delete;
            basic_migration() = delete;

            basic_migration(const std::string& __title, const std::string& __summary, const std::function<bool(const basic_migration<Record>&)>&& __migrate)
                : title(__title), summary(__summary), m_migrate(std::move(__migrate)) {

                uva::database::table* table = uva::database::table::get_table(Record::table()->m_name + "TableMigrations");

                if (!table) throw std::logic_error(std::string("There is no such table '") + Record::table()->m_name + "TableMigrations" + "'");

                id = table->find_by({ { "title", title } });

                m_pending = id == std::string::npos;

                if (is_pending()) {
                     make_label();
                    if (m_migrate(*this)) {
                        date = time(NULL);

                        id = table->create({
                            { "title",   title },
                            { "summary",  summary },
                            { "date",    std::to_string(date) }
                        });
                    }
                }
                else {
                    date = std::stoll(table->at(id, "date"));
                    make_label();
                }
            }
        private:
            void make_label() {
                time_t now = std::time(NULL);

                tm* local_time = std::localtime(&now);

                char buffer[100];

                std::strftime(buffer, 100, "%d%m%Y%H%M%S", local_time);

                label = buffer;
                label += "_" + title;
            }
        public:
            bool is_pending() const {
                return m_pending;
            }
        public:
            void add_column(const std::string& name, const std::string& type, const std::string& default_value) const {

                Record::table()->add_column(name, type, default_value);

            }

            void change_column(const std::string& name, const std::string& type) const
            {
                Record::table()->change_column(name, type);
            }
        };
    };
};