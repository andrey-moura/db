#pragma once

#include <string>
#include <filesystem>
#include <map>
#include <map>
#include <exception>
#include <vector>
#include <iterator>
#include <functional>
#include <thread>
#include <format>
#include "sqlite3.h"

#include <core.hpp>
#include <console.hpp>
#include <diagnostics.hpp>
#include <string.hpp>
// record(record&& other) : uva::database::basic_active_record(std::forward<std::map<std::string, var>>(other.values))\
// {\
//     other.id = 0;\
// }\

#define UVA_DATABASE_AVAILABLE

#define uva_database_params(...) __VA_ARGS__ 

#define uva_database_declare(record) \
public:\
    record() : uva::database::basic_active_record(std::string::npos) { } \
    record(const record& other) : uva::database::basic_active_record(other) { } \
    record(size_t id) : uva::database::basic_active_record(id) { } \
    record(const std::map<std::string, var>& values) : uva::database::basic_active_record(values) { } \
    record(std::map<std::string, var>&& values) : uva::database::basic_active_record(std::forward<std::map<std::string, var>>(values)) { } \
    const uva::database::table* get_table() const override { return table(); } \
    uva::database::table* get_table() override { return table(); } \
    static uva::database::table* table(); \
    static size_t create() { return table()->create(); } \
    static record create(std::map<std::string, var>&& relations) {\
        record r(std::forward<std::map<std::string, var>>(relations)); \
        r.save();\
        r = record::find_by("id={}", r["id"]);\
        return r;\
    } \
    static record create(const std::map<std::string, var>& relations) {\
        record r(relations); \
        r.save();\
        r = record::find_by("id={}", r["id"]);\
        return r;\
    } \
    static void create(std::vector<std::map<std::string, var>>& relations) { table()->create(relations); } \
    static size_t column_count() { return table()->m_columns.size(); } \
    static std::vector<std::pair<std::string, std::string>>& columns() { return table()->m_columns; } \
    static uva::database::active_record_relation all() { return uva::database::active_record_relation(table()).select("*").from(table()->m_name);  } \
    template<class... Args> static uva::database::active_record_relation where(const std::string where, Args const&... args) { return record::all().where(where, args...); }\
    static uva::database::active_record_relation from(const std::string& from) { return record::all().from(from); } \
    static uva::database::active_record_relation select(const std::string& select) { return record::all().select(select); } \
    static size_t count(const std::string& count = "*") { return record::all().count(); } \
    template<class... Args> static uva::database::active_record_relation order_by(const std::string order, Args const&... args) { return record::all().order_by(order, args...); }\
    static uva::database::active_record_relation limit(const std::string& limit) { return record::all().limit(limit); } \
    static uva::database::active_record_relation limit(const size_t& limit) { return record::all().limit(limit); } \
    static void each_with_index(std::function<void(std::map<std::string, var>&, const size_t&)> func) { return record::all().each_with_index(func); }\
    static void each(std::function<void(std::map<std::string, var>&)> func) { return record::all().each(func); }\
    static void each_with_index(std::function<void(record&, const size_t&)> func) { return record::all().each_with_index<record>(func); }\
    static void each(std::function<void(record&)> func) { return record::all().each<record>(func); }\
    template<class... Args> static record find_by(std::string where, Args const&... args) { return record(record::all().find_by(where, args...)); }\
    static record first() { return all().first(); }\
    record& operator=(const record& other)\
    {\
        id = other.id;\
        values = other.values;\
        return *this;\
    }\
    virtual const std::string& class_name() const override\
    {\
        static const std::string class_name = #record;\
        return class_name;\
    };\

#define uva_database_define_full(record, __table_name, sufix) \
uva::database::table* record::table() { \
\
    static std::string table_name = uva::string::to_snake_case(__table_name) + sufix; \
\
    static uva::database::table* table = uva::database::table::get_table(table_name);\
    \
    return table; \
}\

#define uva_database_define(record) uva_database_define_full(record, #record, "s")

#define uva_database_define_sqlite3(db) uva::database::sqlite3_connection* connection = new uva::database::sqlite3_connection(db);

#define uva_declare_migration(migration) public: migration()

#define uva_define_migration(m) m::m() : uva::database::basic_migration(#m) {  } m* _##m = new m();

#define uva_run_migrations() uva::database::basic_migration::do_pending_migrations();

namespace uva
{
    namespace database
    {        
        // Default value of query_buffer_lenght. Don't change this. You can change query_buffer_lenght.
        static constexpr size_t query_buffer_lenght_default = 1024;
        // Increase this if you are going to do big inserts/updates. Helps a lot with performance.
        // You can change it to a big value and then go back to default. The buffer is reset in the next run.
        extern size_t query_buffer_lenght;
        class table;
        class basic_active_record;

        class basic_connection
        {
        private:
            static basic_connection* s_connection;
        public:
            basic_connection();
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
            static basic_connection* get_connection();
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

        extern std::map<std::string, var::var_type> sql_values_types_map;
        const var::var_type& sql_delctype_to_value_type(const std::string& type);

        class active_record_relation
        {
        public:
            active_record_relation() = default;
            active_record_relation(table* table);
        private:
            std::map<std::string, var> m_update;
            std::string m_select;
            std::string m_from;
            std::string m_where;
            std::string m_order;
            std::string m_limit;
            std::vector<std::vector<var>> m_insert;
            std::vector<std::string> m_columns;
            std::string m_into;
            std::string m_returning;
            table* m_table;

            std::map<std::string, size_t> m_columnsIndexes;
            std::vector<std::string> m_columnsNames;
            std::vector<var::var_type> m_columnsTypes;


            bool m_unscoped = false;
        public:
            std::vector<std::vector<var>> m_results;
            std::string to_sql() const;
        public:
            active_record_relation& update(const std::map<std::string, var>& update);
            active_record_relation& select(const std::string& select);
            active_record_relation& from(const std::string& from);
            template<class... Args>
            active_record_relation& where(const std::string where, Args... args)
            {
                #ifdef USE_FMT_FORMT
                    std::string __where = vformat(where, std::make_format_args(args...));
                #else
                    std::string __where = std::format(where, std::forward<Args>(args)...);
                #endif
                append_where(__where);
                return *this;
            }
            template<class... Args>
            active_record_relation& order_by(const std::string order, Args... args)
            {
                #ifdef USE_FMT_FORMT
                    std::string __order = vformat(order, std::make_format_args(args...));
                #else
                    std::string __order = std::format(order, std::forward<Args>(args)...);
                #endif

                order_by(__order);
                return *this;
            }
            active_record_relation& order_by(const std::string& order);
            active_record_relation& limit(const std::string& limit);
            active_record_relation& limit(const size_t& limit);
            active_record_relation& insert(std::vector<var>& insert);
            active_record_relation& insert(std::vector<std::vector<var>>& insert);
            active_record_relation& columns(const std::vector<std::string>& cols);
            active_record_relation& into(const std::string& into);
            active_record_relation& returning(const std::string& returning);
            std::vector<var> run_sql(const std::string& col);
            std::vector<var> pluck(const std::string& col);
            std::vector<std::vector<var>> pluckm(const std::string& cols);
            size_t count(const std::string& count = "*") const;
            std::map<std::string, var> first();
            void each_with_index(std::function<void(std::map<std::string, var>&, const size_t&)> func);
            void each(std::function<void(std::map<std::string, var>&)> func);
            template<class record>
            void each_with_index(std::function<void(record& value, const size_t&)>& func)
            {
                each_with_index([&](std::map<std::string, var>& value, const size_t& index){
                    record r(std::move(value));
                    func(r, index);
                });
            }
            template<class record>            
            void each(std::function<void(record&)>& func)
            {
                each([&](std::map<std::string, var>& value){
                    record r(std::move(value));
                    func(r);
                });
            }
            template<class... Args>
            std::map<std::string, var> find_by(std::string where, Args... args)
            {
                return active_record_relation(*this).where(where, std::forward<Args>(args)...).first();
            }
        public:
            bool empty();
            std::map<std::string, var> operator[](const size_t& index);
            active_record_relation unscoped();
        public:
            void append_where(const std::string& where);
            std::string commit_sql() const;
            void commit();
            void commit_without_prepare();
            void commit(const std::string& sql);
            void commit_without_prepare(const std::string& sql);
        };

        class table
        {
        public:
            table(const std::string& name, const std::vector<std::pair<std::string, std::string>>& cols);
            table(const std::string& name);

            std::string m_name;
            std::vector<std::pair<std::string, std::string>> m_columns;
            std::map<size_t, std::map<std::string, std::string>> m_relations;
            size_t create();
            size_t create(const std::map<std::string, var>& relations);
            void create(std::vector<std::map<std::string, var>>& relations);
            void create(std::vector<std::vector<var>>& relations, const std::vector<std::string>& columns);
            size_t find(size_t id) const;
            size_t find_by(const std::map<std::string, std::string>& relations);
            size_t first();
            size_t last();
            std::string primary_key;
            void destroy(size_t id);
            bool relation_exists(size_t id) const;
            void update(size_t id, const std::string& key, const std::string& value);
            void update(size_t id, const std::map<std::string, var>& value);
            static std::map<std::string, table*>& get_tables();
            static table* get_table(const std::string& name);
            std::string& at(size_t id, const std::string& key);
            void add_column(const std::string& name, const std::string& type, const std::string& default_value);
            void change_column(const std::string& name, const std::string& type);
            std::vector<std::pair<std::string, std::string>>::const_iterator find_column(const std::string& col) const;
            std::vector<std::pair<std::string, std::string>>::iterator find_column(const std::string& col);
            static void add_table(uva::database::table* table);
        };
        
        class basic_active_record
        {              
        public:
            basic_active_record(size_t _id);
            basic_active_record(const basic_active_record& record);
            basic_active_record(basic_active_record&& record);
            basic_active_record(const std::map<std::string, var>& value);
            basic_active_record(std::map<std::string, var>&& value);
        public:
            bool present() const;
            void destroy();            
        public:
            size_t id = 0; 
        protected:
            virtual const table* get_table() const = 0;
            virtual table* get_table() = 0;
            virtual void before_save() { };
            virtual void before_update() { };
            virtual const std::string& class_name() const = 0;
            std::string to_s() const;
            std::map<std::string, var> values;
        public:
            basic_active_record& operator=(const basic_active_record& other);
        public:
            var& at(const std::string& str);
            const var& at(const std::string& str) const;

            void save();
            void update(const std::string& col, const var& value);
            void update(const std::map<std::string, var>& values);
        public:
            var& operator[](const char* str);
            const var& operator[](const char* str) const;
            var& operator[](const std::string& str);
            const var& operator[](const std::string& str) const;
        };
        
        class basic_migration : public basic_active_record
        {
        uva_database_declare(basic_migration);
        public:
            basic_migration(const std::string& __title);
        private:
            std::string make_label();
            std::string title;
        public:
            bool is_pending();
            virtual void change() {};
            void apply();
        public:
            static std::vector<basic_migration*>& get_migrations();
            static void do_pending_migrations();
        protected:
            void call_change();
        public:
            void add_table(const std::string& table_name, const std::vector<std::pair<std::string, std::string>>& cols);
            void drop_table(const std::string& table_name);
            void add_column(const std::string& table_name, const std::string& name, const std::string& type, const std::string& default_value) const;
            void add_index(const std::string& table_name, const std::string& column);
            void change_column(const std::string& table_name, const std::string& name, const std::string& type) const;
        };
    };
};