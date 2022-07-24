#include <cspec.hpp>
#include <database.hpp>

class Album : public uva::database::basic_active_record
{
    uva_database_declare(Album);
};

CSPEC_FOLDER

uva_database_define_sqlite3(Album,
{

}, "")

cspec_describe("Connecting to a table",

    it("to_sql should match", [](){
        
    })

);

cpsec_configure()