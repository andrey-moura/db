#include <uva/db.hpp>
#include <faker.hpp>
#include <cspec.hpp>

using namespace uva::db;

class Product : public basic_active_record
{
    uva_database_declare(Product);
protected:
    virtual void before_save() override
    {
        std::cout << "before_save product\n";
    }
    virtual void before_update() override
    {
        std::cout << "before_update product\n";
    }

    void print_columns()
    {
        std::cout << uva::string::join(uva::string::join(values, [](const std::pair<std::string, multiple_value_holder>& value) {
            return std::format("{}={}", value.first, value.second.to_s());
        }), ',');
    }
};

class MultipleValueHolder : public basic_active_record
{
uva_database_declare(MultipleValueHolder);
};

uva_db_define(Product);
uva_db_define(MultipleValueHolder);

class AddProductsMigration : public basic_migration
{
    uva_db_declare_migration(AddProductsMigration);
public:
    virtual void change() override
    {
        add_table("products",
        {
            { "name", "TEXT" },
            { "price", "REAL" }
        });
    }
};

class AddMultipleValueHoldersMigration : public basic_migration
{
    uva_db_declare_migration(AddMultipleValueHoldersMigration);
public:
    virtual void change() override
    {
        add_table("multiple_value_holders",
        {
            { "string",  "TEXT" },
            { "integer", "INTEGER" },
            { "float",   "REAL" },
            { "null_value",    "INTEGER" },
        });
    }
};

uva_db_define_migration(AddProductsMigration)
uva_db_define_migration(AddMultipleValueHoldersMigration)

static std::filesystem::path database_path;

cspec_describe("uva::db",

    context("setting up a new database", 

        before_all_tests([](){
            database_path = uva::cspec::temp_folder / "database.db";
        })

        it("should start without touching the database", []()
        {
            expect(database_path).to_not exist;
        })

        it("should create a new database after uva_db_define", []()
        {
           uva_db_define_sqlite3(database_path);
           expect(database_path).to exist;
        })

        it("should starts without creating AddProductMigration migration", []()
        {
           expect(basic_migration::where("title='{}'", "AddProductsMigration")).to_not exist;
        })

        it("should create AddProductMigration migration after uva_db_run_migrations", []()
        {
           uva_db_run_migrations()
           expect(basic_migration::where("title='{}'", "AddProductsMigration")).to exist;
        })

        it("should create new product", []()
        {
           std::string product = uva::faker::commerce::product();
           double price = uva::faker::commerce::price();

           Product::create({
               { "name", product },
               { "price",   price }
           });

           expect(Product::count()).to eq(1);
           Product created_product = Product::first();

           bool e = created_product["name"] == product;

           expect<std::string>(created_product["name"]).to eq(product);
           expect(created_product["price"]).to eq(price); 
        })

        it("should update product", []()
        {
            Product product = Product::create({
                { "name", uva::faker::commerce::product() },
                { "price", uva::faker::commerce::price() }
            });

            // Product saved_product = Product::find_by("id={}", product["id"]);
            // expect(saved_product["name"]).to eq(product["name"]);
            // expect(saved_product["price"]).to eq(product["price"]);
        })
    )

    context("selecting values", 

        before_all_tests([](){

            Product::create({
                { "name", "Perfume" },
                { "price", 1.8 },
            });

            Product::create({
                { "name", "Book" },
                { "price", 7.99 },
            });

            Product::create({
                { "name", "Mobile Phone" },
                { "price", 499 },
            });

            Product::create({
                { "name", "Notebook" },
                { "price", 1999 },
            });

            Product::create({
                { "name", "Deer" },
                { "price", 5 },
            });
        })

        it("should pluck last 5 products.name", [](){
            std::vector<var> values = Product::all().order_by("id desc").limit(5).pluck("name");

            expect(values).to eq(std::vector<std::string>({
                "Deer", "Notebook", "Mobile Phone", "Book", "Perfume"
            }));
        })
    )

    context("callbacks",
        it("should call before_save on new record", [](){
            expect([](){
                Product::create({
                    { "name", uva::faker::commerce::product() },
                    { "price", uva::faker::commerce::price() }
                });
            }).to log_on_cout("before_save product");
        })

        it("should NOT call before_update on new record", [](){
            expect([](){
                Product::create({
                    { "name", uva::faker::commerce::product() },
                    { "price", uva::faker::commerce::price() }
                });
            }).to_not log_on_cout("before_update product");
        })

        it("should call before_save on new record and before_update on change", [](){
            Product product;

            expect([&product](){
                product = Product::create({
                    { "name", uva::faker::commerce::product() },
                    { "price", uva::faker::commerce::price() }
                });
            }).to log_on_cout("before_save product");

            expect([&product](){
                product.update("name", uva::faker::commerce::product());
            }).to log_on_cout("before_update product");
        })

        it("should call before_update AND before_save on change", [](){
            Product product = Product::create({
                { "name", uva::faker::commerce::product() },
                { "price", uva::faker::commerce::price() }
            });

            expect([&product](){
                product.update("name", uva::faker::commerce::product());
            }).to log_on_cout({"before_update product", "before_save product"});
        })
    )

    context("types",
        it("should read correct values and their types", [](){

            std::string string = uva::faker::lorem::sentence();
            int integer = uva::faker::random_integer(-100000, 100000);
            double floating = uva::faker::random_double(-50.11, 50.33);

            MultipleValueHolder::create({
                { "string",  string },
                { "integer", integer },
                { "float",   floating },
                { "null_value",    null },
            });

            MultipleValueHolder product = MultipleValueHolder::first();

            expect(product["string"]).to eq(string);
            expect(product["integer"]).to eq(integer);
            expect(product["float"]).to eq(floating);
            expect(product["null_value"]).to eq(null);

            expect(product["string"].type).to eq(multiple_value_holder::value_type::string);
            expect(product["integer"].type).to eq(multiple_value_holder::value_type::integer);
            expect(product["float"].type).to eq(multiple_value_holder::value_type::real);
            expect(product["null_value"].type).to eq(multiple_value_holder::value_type::null_type);

        })
    )
);

// cspec_describe("Reading values",

//     it("query with select should match", [](){        

//         auto values = Album::select("name");
        
//         expect(values.to_sql()).to() << eq("SELECT name FROM albums WHERE removed = 0;");

//     })

//     it("query with where and format should match", [](){        

//         auto values = Album::where("title = '{}'", "Górecki: Symphony No. 3");
        
//         expect(values.to_sql()).to() << eq("SELECT * FROM albums WHERE title = 'Górecki: Symphony No. 3' AND removed = 0;");

//     })

//     it("query with order by should match", [](){
//         auto values = Album::order_by("id");

//         expect(values.to_sql()).to() << eq("SELECT * FROM albums WHERE removed = 0 ORDER BY id;");
//     })

//     it("query with limit should match", [](){
//         auto values = Album::limit(3);

//         expect(values.to_sql()).to() << eq("SELECT * FROM albums WHERE removed = 0 LIMIT 3;");
//     })

//     it("where selecting first should match values", [](){
//         auto values = Album::where("title = '{}'", "Restless and Wild").first();

//         size_t      albumId  = values["AlbumId"];
//         std::string title    = values["Title"];
//         size_t      artistId = values["ArtistId"];
//         bool        removed  = values["removed"];

//         expect(albumId).to()  << eq(3);
//         expect(title).to()    << eq("Restless and Wild");
//         expect(artistId).to() << eq(2);
//         expect(removed).to()  << eq(false);
//     })

//     it("looping into values AlbumId should math index", [](){
//         Album::each_with_index([](Album& album, const size_t& index) {
//             size_t id = album["AlbumId"];
//             expect(id).to() << eq(index+1);
//         });
//     })

//     it("count should eq 347", [](){
//         size_t count = Album::count();
        
//         expect(count).to() << eq(347);
//     })
// );

cspec_configure()