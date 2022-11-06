#include <fstream>

#include <core.hpp>
#include <string.hpp>
#include <console.hpp>
#include <database.hpp>

using namespace uva::console;

void create_migration(const std::filesystem::path& app_path, const std::string& name, const std::string& change)
{
    var now_time_str = now().strftime("%Y%m%d%H%M%S");

    var migration_file_name_format = "{}_{}.cpp";
    var migration_file_name = migration_file_name_format.format(now_time_str, uva::string::to_snake_case(name));
    
    std::filesystem::path migration_path = app_path / "migrations" / migration_file_name;

    std::ofstream migration_file(migration_path);
	if (!migration_file.is_open()) {
		std::cout << "error: unable to create file " << migration_path.string() << std::endl;
		return;
	}

    var includes = {"core.hpp", "database.hpp"};
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

    var migration_content = migration_format.format(name, name, change, name);

    migration_file << migration_content;

    migration_file.close();
}

void create_model(const std::filesystem::path& app_path, const std::string& name)
{
    var file_name = uva::string::to_snake_case(name);
   
    std::filesystem::path model_header_path = app_path / "models" / file_name;
    std::filesystem::path model_source_path = model_header_path;
    
    model_header_path.replace_extension("hpp");
    model_source_path.replace_extension("cpp");

    std::ofstream model_header_file(model_header_path);
	if (!model_header_file.is_open()) {
		std::cout << "error: unable to create file " << model_header_path.string() << std::endl;
		return;
	}

    var includes = {"core.hpp", "database.hpp"};
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

std::filesystem::path get_app_path()
{
    std::filesystem::path app_path;

    if(has_args()) {
        app_path = front_arg();
    } else {
        app_path = std::filesystem::absolute("app");

        // remove in future
        if(!std::filesystem::exists(app_path)) {
            app_path = std::filesystem::absolute("src");
        }

        if(!std::filesystem::exists(app_path)) {
            log_error("error: failed to find app folder in {}", app_path.string());
        }
    }

    return app_path;
}

void generate_model()
{
    if(uva::console::args_count() < 1) {
        log_error("error: expected 1 arguments, got {}", uva::console::args_count());
        return;
    }

    var model_name = front_arg();
    var table_name = model_name.pluralize().downcase();

    model_name = model_name.capitalize();

    std::filesystem::path app_path = get_app_path();

    var migration_name_format = "Add{}Migration";
    var migration_content_format =
R"~~~(
        add_table("{}",
        {{
            {{ "updated_at", "INTEGER DEFAULT (STRFTIME('%s'))" }},
            {{ "created_at", "INTEGER DEFAULT (STRFTIME('%s'))" }},
            {{ "removed",    "INTEGER DEFAULT 0" }},
        }});
)~~~";

    var migration_name = migration_name_format.format(model_name.pluralize());
    var migration_content = migration_content_format.format(table_name);

    create_migration(app_path, migration_name, migration_content);
    create_model(app_path, model_name);

}

void generate_migration()
{
    if(uva::console::args_count() < 1) {
        log_error("error: expected 1 arguments, got {}", uva::console::args_count());
        return;
    }

    var migration_name = uva::console::front_arg();
    auto app_path = get_app_path();

    create_migration(app_path, migration_name, "");
}

int main(int argc, char** argv)
{
    uva::console::init_args(argc, argv);

    if(!uva::console::has_args()) {
        log_error("missing arguments");
        return 0;
    }

    auto arg = uva::console::front_arg();

    if(arg == "generate")
    {
        arg = uva::console::front_arg();

        if(arg == "model")
        {
            generate_model();
        } else if(arg == "migration")
        {
            generate_migration();
        }
    }
}