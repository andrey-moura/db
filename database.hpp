#pragma once

#include "sqlite3.h"
#include <string>
#include <filesystem>
#include <map>
#include <map>
#include <exception>
#include <vector>
#include <iterator>
#include <functional>

#define uva_database_params(...) __VA_ARGS__ 

// #define uva_database_declare(record) \
// public:\    
//     static table s_table; \
//     static basic_connection* s_connection;\
//     virtual table* get_table() override {\
//        return s_table; \
//     }\
//     virtual basic_connection* get_connection() {\
// \
//     }

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
    static size_t find(size_t id) { return table()->find(id); }  \
    static size_t find_by(const std::map<std::string,std::string>& relations) { return table()->find_by(relations); } \
    static uva::database::active_record_collection where(const std::map<std::string,std::string>& relations) { return table()->where(relations); } \
    static uva::database::active_record_iterator<record> begin() { return uva::database::active_record_iterator<record>(table()->m_relations.begin()); } \
	static uva::database::active_record_iterator<record> end() { return uva::database::active_record_iterator<record>(table()->m_relations.end()); } \
    static uva::database::active_record_reverse_iterator<record> rbegin() { return uva::database::active_record_reverse_iterator<record>(table()->m_relations.rbegin()); } \
	static uva::database::active_record_reverse_iterator<record> rend() { return uva::database::active_record_reverse_iterator<record>(table()->m_relations.rend()); } \
    static size_t count() { return table()->m_relations.size(); } \
    static size_t column_count() { return table()->m_rows.size(); } \
    static size_t first() { return table()->first(); } \

    //std::string& operator[](const std::string& str) { return m_table[str]; }
    // #define uva_database_define(record, params) \
     //std::map<std::string, std::string> record::s_table = params; 
     //bool record::s_initialized = false;

#define uva_database_define_sqlite3(record, rows, db) \
uva::database::table* record::table() { \
\
    uva::database::sqlite3_connection* connection = uva::database::sqlite3_connection::connect(db);\
\
    static uva::database::table* table = new uva::database::table(std::string(#record) + "s", rows, connection); \
    static uva::database::table* infotable = new uva::database::table(std::string(#record) + "s" + "TableMigrations", { \
        { "title", "TEXT" },\
        { "summary", "TEXT" },\
        { "date", "INTEGER" }\
    }, connection); \
    return table; \
}

#define uva_database_declare_migration(record) using record##Migration = uva::database::basic_migration<Beach>

namespace uva
{
    namespace database
    {        
        class table;
        class relation;
        class basic_active_record;
        class basic_active_record_collections;

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
                static sqlite3_connection* connect(const std::filesystem::path& database);
                virtual bool open() override;
                virtual bool is_open() const override;
                bool open(const std::filesystem::path& path);
                virtual bool create_table(const table* table) const override;
                virtual void read_table(table* table) override;
                virtual bool insert(table* table, size_t id, const std::map<std::string, std::string>& relations) override;
                virtual bool insert(table* table, size_t id, const std::vector<std::map<std::string, std::string>>& relations) override;
                virtual void update(size_t id, const std::string& key, const std::string& value, table* table) override;
                virtual void destroy(size_t id, uva::database::table* table) override;
                virtual void add_column(uva::database::table* table, const std::string& name, const std::string& type, const std::string& default_value) override;
        };

        class active_record_collection
        {
        private:
            std::vector<size_t> m_matches;
        public:            
            auto begin() { return m_matches.begin(); }
            auto end() { return m_matches.end(); }
            size_t size() { return m_matches.size(); }
            void reserve(size_t len) { m_matches.reserve(len); }
        public:
            active_record_collection& operator<<(size_t r);
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
            friend std::string operator+(const std::string& other, value& value);
            friend std::ostream& operator<<(std::ostream& out, value& value)
            {
                out << value.operator std::string & ();
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
                const std::map<std::string, std::string>& rows,
                basic_connection* connection);

            std::string m_name;
            basic_connection* m_connection;
            std::map<std::string, std::string> m_rows;
            std::map<size_t, std::map<std::string, std::string>> m_relations;
            size_t create();
            size_t create(const std::map<std::string, std::string>& relations);
            void create(std::vector<std::map<std::string, std::string>>& relations);
            size_t find(size_t id) const;
            size_t find_by(const std::map<std::string, std::string>& relations);
            size_t first();
            void destroy(size_t id);
            bool relation_exists(size_t id) const;
            void update(size_t id, const std::string& key, const std::string& value);
            active_record_collection where(const std::map<std::string, std::string>& relations);
            static std::map<std::string, table*>& get_tables();
            static table* get_table(const std::string& name);
            std::string& at(size_t id, const std::string& key);
            void add_column(const std::string& name, const std::string& type, const std::string& default_value);
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
        };
        template<typename record>
        class active_record_iterator
        {
        private:
            std::map<size_t, std::map<std::string, std::string>>::iterator m_iterator;
        public:
            active_record_iterator(const std::map<size_t, std::map<std::string, std::string>>::iterator& it)
                : m_iterator(it) {        
            }
        public:
            void advance(int n) {
                std::advance(m_iterator, n);
            }
        public:
            active_record_iterator<record>& operator++() {
                m_iterator++;
                return *this;
            }            
            record operator*() {
                return record(m_iterator->first);
            }
            active_record_iterator<record> operator+(const size_t& i) {
                auto it = m_iterator;
                std::advance(it, i);
                return active_record_iterator(it);
            }
            bool operator!=(const active_record_iterator<record>& other) {
                return m_iterator != other.m_iterator;
            }
            bool operator==(const active_record_iterator<record>& other) {
                return m_iterator == other.m_iterator;
            }
        };
        template<typename record>
        class active_record_reverse_iterator
        {
        private:
            std::map<size_t, std::map<std::string, std::string>>::reverse_iterator m_iterator;
        public:
            active_record_reverse_iterator(const std::map<size_t, std::map<std::string, std::string>>::reverse_iterator& it)
                : m_iterator(it) {

            }  
        public:
            void advance(int n) {
                std::advance(m_iterator, n);
            }
        public:
            active_record_reverse_iterator<record>& operator++() {
                m_iterator++;
                return *this;
            }
            active_record_reverse_iterator<record> operator+(const size_t& i) {
                auto it = m_iterator;
                std::advance(it, i);
                return active_record_reverse_iterator(it);
            }
            record operator*() {
                return record(m_iterator->first);
            }
            bool operator!=(const active_record_reverse_iterator<record>& other) {
                return m_iterator != other.m_iterator;
            }
            bool operator==(const active_record_reverse_iterator<record>& other) {
                return m_iterator == other.m_iterator;
            }
        };
    };
};