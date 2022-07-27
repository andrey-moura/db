#include <cspec.hpp>
#include <database.hpp>

class Album : public uva::database::basic_active_record
{
    uva_database_declare(Album);
};

uva_database_define_sqlite3(Album, uva_database_params(
{
    { "AlbumId", "INTEGER" },
    { "Title",   "TEXT" },
    { "ArtistId", "INTEGER" }
}), std::filesystem::path(CSPEC_FOLDER) / "chinook.db");

cspec_describe("Reading values",

    it("count should eq 347", [](){

        size_t count = Album::count();
        
        expect(count).to() << eq(347);
    })

    it("select query should match", [](){        

        auto values = Album::select("name");
        
        expect(values.to_sql()).to() << eq("SELECT name FROM albums WHERE removed=0;");

    })

);

cpsec_configure()