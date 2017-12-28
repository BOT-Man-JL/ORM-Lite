
// Test of ORM Lite
// https://github.com/BOT-Man-JL/ORM-Lite
// BOT Man, 2016

#include <iostream>
#include <memory>
#include <string>

#include "../src/ormlite.h"
using namespace BOT_ORM;
using namespace BOT_ORM::Expression;

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#define TESTDB "test.db"

struct ModelA
{
    int a_int;
    std::string a_string;
    double a_double;

    Nullable<int> an_int;
    Nullable<double> an_double;
    Nullable<std::string> an_string;

    // Inject ORM-Lite into this Class :-)
    ORMAP ("ModelA", a_int, a_string, a_double, an_int, an_double, an_string);
};

struct ModelB
{
    unsigned long b_ulong;
    float b_float;

    Nullable<unsigned long> bn_ulong;
    Nullable<float> bn_float;

    // Inject ORM-Lite into this Class :-)
    ORMAP ("ModelB", b_ulong, b_float, bn_ulong, bn_float);
};

struct ModelC
{
    unsigned c_uint;
    int a_int;
    unsigned long b_ulong;

    // Inject ORM-Lite into this Class :-)
    ORMAP ("ModelC", c_uint, a_int, b_ulong);
};

struct ModelD
{
    int d_int;

    // Inject ORM-Lite into this Class :-)
    ORMAP ("ModelD", d_int);
};

namespace detail
{
    template<typename Model>
    void ResetTable (const Model &model)
    {
        ORMapper mapper (TESTDB);

        try {
            mapper.CreateTbl (model);
        }
        catch (...) {
            mapper.DropTbl (model);
            mapper.CreateTbl (model);
        }
    }
}

void ResetTables () {}

template<typename Model, typename ...OtherModels>
void ResetTables (const Model &model, const OtherModels &...models)
{
    detail::ResetTable (model);
    ResetTables (models...);
}

TEST_CASE ("create/drop tables")
{
    ResetTables (ModelA {}, ModelB {}, ModelC {}, ModelD {});

    ORMapper mapper (TESTDB);

    mapper.DropTbl (ModelA {});
    mapper.DropTbl (ModelB {});
    mapper.DropTbl (ModelC {});
    mapper.DropTbl (ModelD {});

    mapper.CreateTbl (ModelA {});
    mapper.CreateTbl (ModelB {});
    mapper.CreateTbl (ModelC {});
    mapper.CreateTbl (ModelD {});
}

TEST_CASE ("normal cases")
{
    ModelA ma;
    ModelD md;
    auto field = FieldExtractor { ma, md };

    ORMapper mapper (TESTDB);

    mapper.Insert (ModelD { 0 });
    mapper.Insert (ModelD { 0 }, false);
    mapper.InsertRange (std::list<ModelD> { ModelD { 2 }, ModelD { 3 } });
    mapper.InsertRange (std::list<ModelD> { ModelD { 2 }, ModelD { 3 } }, false);
    mapper.Update (ModelD { 0 });
    mapper.UpdateRange (std::list<ModelD> { ModelD { 2 }, ModelD { 3 } });
    mapper.Update (ModelD {}, field (md.d_int) = 6, field (md.d_int) == 0);  // 0 -> 6
    mapper.Delete (ModelD { 1 });
    mapper.Delete (ModelD {}, field (md.d_int) == 0);  // No such one

    constexpr auto countExpected = 5;
    constexpr auto firstIdExpected = 2;
    constexpr auto lastIdExpected = 6;
    // Expected: 2, 3, 4, 5, 6

    REQUIRE (mapper.Query (ModelD {})
        .Aggregate (Count ()).Value () == countExpected);
    REQUIRE (mapper.Query (ModelD {})
        .LeftJoin (ModelA {}, field (ma.a_int) == field (md.d_int))
        .Aggregate (Count ()).Value () == countExpected);

    mapper.Insert (ModelA {}, false);
    REQUIRE (mapper.Query (ModelD {}).Select (field (md.d_int))
        .Union (mapper.Query (ModelA {}).Select (field (ma.a_int)))
        .ToList ().size () == countExpected + 1);
    REQUIRE (mapper.Query (ModelD {})
        .ToVector ()[countExpected - 1].d_int == lastIdExpected);
    auto firstTuple = mapper.Query (ModelD {})
        .Select (field (md.d_int))
        .ToList ().front ();
    REQUIRE (std::get<0> (firstTuple).Value () == firstIdExpected);
}

TEST_CASE ("handle existing table")
{
    // before
    ResetTables (ModelD {});
    {
        sqlite3 *db;
        sqlite3_open (TESTDB, &db);
        sqlite3_exec (db,
            "DROP TABLE ModelD;"
            "CREATE TABLE ModelD (d_int INTEGER, d_str TEXT);"
            "INSERT INTO ModelD values (1, 'John');",
            nullptr, nullptr, nullptr);
        sqlite3_close (db);
    }

    // test
    ORMapper mapper (TESTDB);
    REQUIRE_THROWS_WITH (mapper.Query (ModelD {}).ToList (),
        "SQL error: 'Bad Column Count' at 'select * from ModelD;'");
}

TEST_CASE ("chinese characters")
{
    // before
    ResetTables (ModelA {});

    // test
    ORMapper mapper (TESTDB);

    mapper.Insert (
        ModelA { 0, "你好", 0, nullptr, nullptr, nullptr }, false);
    mapper.Insert (
        ModelA { 0, u8"世界", 0, nullptr, nullptr, nullptr }, false);

    auto chinese = mapper.Query (ModelA {}).ToVector ();

    REQUIRE (chinese.size () == 2);
    REQUIRE (chinese[0].a_string == "你好");
    REQUIRE (chinese[1].a_string == u8"世界");
}

TEST_CASE ("lifetime of mapper")
{
    // before
    ResetTables (ModelA {});
    {
        ORMapper mapper (TESTDB);
        mapper.Insert (ModelA {}, false);
        mapper.Insert (ModelA {}, false);
    }

    // test
    std::unique_ptr<Queryable<ModelA>> queryable;
    {
        ORMapper mapper (TESTDB);
        queryable.reset (new Queryable<ModelA> { mapper.Query (ModelA {}) });
    }
    REQUIRE (queryable->ToList ().size () == 2);
}

using InvalidModel = int;

TEST_CASE ("invalid model", "[not-compile]")
{
    ORMapper mapper (TESTDB);

    //mapper.CreateTbl (InvalidModel {});
    //mapper.CreateTbl (InvalidModel {}, Constraint::Unique (Field<int> {"", nullptr}));
    //mapper.DropTbl (InvalidModel {});

    //mapper.Insert (InvalidModel {});
    //mapper.Insert (InvalidModel {}, false);
    ////mapper.InsertRange (InvalidModel {});
    //mapper.InsertRange (std::vector<int> {});
    //mapper.InsertRange (std::vector<int> {}, false);
    //mapper.Update (InvalidModel {});
    ////mapper.UpdateRange (InvalidModel {});
    //mapper.UpdateRange (std::vector<int> {});
    //mapper.Update (InvalidModel {}, SetExpr { "" }, Expr { Selectable<int> {"", nullptr}, "" });
    //mapper.Delete (InvalidModel {});
    //mapper.Delete (InvalidModel {}, Expr { Selectable<int> {"", nullptr}, "" });

    //mapper.Query (InvalidModel {});
    //FieldExtractor { InvalidModel {}, double () };
    ////mapper.Query (ModelA {})
    ////	.Join (InvalidModel {}, Expr { Selectable<int> {"", nullptr}, "" });
    ////mapper.Query (ModelA {})
    ////	.LeftJoin (InvalidModel {}, Expr { Selectable<int> {"", nullptr}, "" });
}

struct SickModel
{
    ModelD dd;
    char ch;
    ORMAP ("SickModel", dd, ch);
};

TEST_CASE ("invalid field", "[not-compile]")
{
    ORMapper mapper (TESTDB);

    //mapper.CreateTbl (SickModel {});
    //mapper.Insert (SickModel {});
    //mapper.Update (SickModel {});
    //mapper.Delete (SickModel {});
    //mapper.Query (SickModel {}).ToList ();
}
