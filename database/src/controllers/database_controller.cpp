#include "database_controller.hpp"

#include <filesystem>
#include <file.hpp>
#include <fstream>

void create_model(const var& name)
{
    var file_name = uva::string::to_snake_case(name);
   
    std::filesystem::path models_include = std::filesystem::absolute("include") / "models";
    std::filesystem::path models_src = std::filesystem::absolute("src") / "models";

    if(!std::filesystem::exists(models_include)) {
        std::filesystem::create_directories(models_include);
    }
    
    if(!std::filesystem::exists(models_src)) {
        std::filesystem::create_directories(models_src);
    }

    std::filesystem::path model_header_path = models_include / file_name;
    std::filesystem::path model_source_path = models_src / file_name;

    model_header_path.replace_extension("hpp");
    model_source_path.replace_extension("cpp");

    std::ofstream model_header_file(model_header_path);
	if (!model_header_file.is_open()) {
		std::cout << "error: unable to create file " << model_header_path.string() << std::endl;
		return;
	}

    var includes = var::array({"core.hpp", "database.hpp"});
    var include_format = "#include <{}>\n";

    includes.each([&](const var& include){
        model_header_file << include_format.format(include);
    });

    var header_format =
R"~~~(
class {} : public uva::database::basic_active_record
{{
public:
    uva_database_declare({});
}};    
)~~~";

    model_header_file << header_format.format(name, name);

    std::ofstream model_source_file(model_source_path);
	if (!model_source_file.is_open()) {
		std::cout << "error: unable to create file " << model_source_path.string() << std::endl;
		return;
	}

    var source_format = "#include \"{}.hpp\"\n\nuva_database_define({});";
    
    model_source_file << source_format.format(file_name, name);
}

void create_migration(const var& name, const var& content)
{
    var now_time_str = now().strftime("%Y%m%d%H%M%S");

    var migration_file_name_format = "{}_{}.cpp";
    var migration_file_name = migration_file_name_format.format(now_time_str, uva::string::to_snake_case(name));
    
    std::filesystem::path migrations_path = std::filesystem::absolute("src") / "migrations";

    if(!std::filesystem::exists(migrations_path)) {
        std::filesystem::create_directories(migrations_path);
    }

    std::filesystem::path migration_path = migrations_path / migration_file_name;

    std::ofstream migration_file(migration_path);
	if (!migration_file.is_open()) {
		std::cout << "error: unable to create file " << migration_path.string() << std::endl;
		return;
	}

    var includes = var::array({"core.hpp", "database.hpp"});
    var include_format = "#include <{}>\n";

    includes.each([&](const var& include){
        migration_file << include_format.format(include);
    });

    migration_file << "\n";

    var migration_format =
R"~~~(
class {} : public uva::database::basic_migration
{{
uva_declare_migration({});
protected:
    virtual void change() override 
    {{ {}
    }}
}};

uva_define_migration({});
)~~~";

    var migration_content = migration_format.format(name, name, content, name);

    migration_file << migration_content;

    migration_file.close();
}

void database_controller::init()
{
    var file_path = params[0];

    if(!file_path) {
        log_error("missing parameter 'file path'");
        return;
    }

    if(!std::filesystem::exists(file_path.to_s())) {
        log_error("file {} does not exists", file_path);
        return;
    }

    var start = params[1];

    if(!start) {
        log_error("missing parameter start");
        return;
    }

    //these must be parameters
    std::string engine = "sqlite3";
    std::string path = "db";
    std::string database_name = "database";

    const char* const database_defination_format =
    "\n\tuva_database_define_{}(std::filesystem::path(\"{}\") / \"{}.db\");\n\tuva_run_migrations();\n";

    std::string database_defination = std::format(database_defination_format, engine, path, database_name);

    uva::file::insert_text(file_path.to_s(), start.to_i(), database_defination);
}

void database_controller::new_model()
{
    var name = params[0];

    if(!name) {
        log_error("missing parameter name");
        return;
    }

    var table_name = name.pluralize().downcase();
    var model_name = name.capitalize();

    create_model(model_name);

    if(params.key("--migrate") != null) {
        var migration_name_format = "Add{}Migration";
        var migration_content_format =
    R"~~~(
        add_table("{}",
        {{
            {{ "id",         "INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL" }},
            {{ "updated_at", "INTEGER NOT NULL DEFAULT (STRFTIME('%s'))"  }},
            {{ "created_at", "INTEGER NOT NULL DEFAULT (STRFTIME('%s'))"  }},
            {{ "removed",    "INTEGER NOT NULL DEFAULT 0"                 }},
        }});
    )~~~";

        var migration_name = migration_name_format.format(model_name.pluralize());
        var migration_content = migration_content_format.format(table_name);

        create_migration(migration_name, migration_content);
    }
}

void database_controller::new_migration()
{
    var name = params[0];

    if(!name) {
        log_error("missing parameter name");
        return;
    }

    create_migration(name, "");
}