#include <database.hpp>
#include <faker.hpp>
#include <cspec.hpp>

class Product : public uva::database::basic_active_record
{
    uva_database_declare(Product);
};

uva_database_define(Product);

class AddProductMigration : public uva::database::basic_migration
{
    uva_declare_migration(AddProductMigration);
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

uva_define_migration(AddProductMigration)

std::filesystem::path database_path;

cspec_describe("uva::database",

    context("setting up a new database", 

        before_all_tests([&](){
            database_path = uva::cspec::temp_folder / "database.db";
        })

        it("should start without touching the database", []()
        {
            expect(database_path).to_not exist;
        })

        it("should create a new database after uva_database_define", []()
        {
           uva_database_define_sqlite3(database_path);
           expect(database_path).to exist;
        })

        it("should starts without creating AddProductMigration migration", []()
        {
           expect(uva::database::basic_migration::where("title='{}'", "AddProductMigration")).to_not exist;
        })

        it("should create AddProductMigration migration after uva_run_migrations", []()
        {
           uva_run_migrations()
           expect(uva::database::basic_migration::where("title='{}'", "AddProductMigration")).to exist;
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
            std::vector<uva::database::multiple_value_holder> values = Product::all().order_by("id desc").limit(5).pluck("name");

            expect(values).to eq(std::vector<std::string>({
                "Deer", "Notebook", "Mobile Phone", "Book", "Perfume"
            }));
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