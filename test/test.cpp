﻿
// Test of ORM Lite
// https://github.com/BOT-Man-JL/ORM-Lite
// BOT Man, 2016

#include <string>
#include <iostream>
#include <assert.h>
#include <time.h>

#include "../src/ormlite.h"
using namespace BOT_ORM;
using namespace BOT_ORM::Expression;

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

struct ModelE
{
    ModelD dd;
    char ch;

    // Invalid Injection
    ORMAP ("ModelE", dd, ch);
};

int main ()
{
    auto begTime = time (nullptr);
    ORMapper mapper (TESTDB);

    // Create Brand New Tables
    auto initTables = [&mapper] ()
    {
        auto initTable = [&mapper] (const auto &model)
        {
            try
            {
                mapper.CreateTbl (model);
            }
            catch (...)
            {
                mapper.DropTbl (model);
                mapper.CreateTbl (model);
            }
        };
        initTable (ModelA {});
        initTable (ModelB {});
        initTable (ModelC {});
        initTable (ModelD {});
    };
    initTables ();

    //
    // Case: Not Compile - NO ORMAP
    //
    {
        //mapper.CreateTbl (int ());
        //mapper.CreateTbl (int (), Constraint::Unique (Field<int> {"", nullptr}));
        //mapper.DropTbl (int ());
        //mapper.Insert (int ());
        //mapper.Insert (int (), false);
        ////mapper.InsertRange (int ());
        //mapper.InsertRange (std::vector<int> ());
        //mapper.InsertRange (std::vector<int> (), false);
        //mapper.Update (int ());
        ////mapper.UpdateRange (int ());
        //mapper.UpdateRange (std::vector<int> ());
        //mapper.Update (int (), SetExpr { "" }, Expr { Selectable<int> {"", nullptr}, "" });
        //mapper.Delete (int ());
        //mapper.Delete (int (), Expr { Selectable<int> {"", nullptr}, "" });
        //mapper.Query (int ());
        //FieldExtractor { int (), double () };
        ////mapper.Query (ModelA {})
        ////	.Join (int (), Expr { Selectable<int> {"", nullptr}, "" });
        ////mapper.Query (ModelA {})
        ////	.LeftJoin (int (), Expr { Selectable<int> {"", nullptr}, "" });
    }

    //
    // Case: Not Compile - Not Support Field
    //
    {
        //mapper.CreateTbl (ModelE {});
        //mapper.Insert (ModelE {});
        //mapper.Update (ModelE {});
        //mapper.Delete (ModelE {});
        //mapper.Query (ModelE {}).ToList ();
    }

    //
    // Case: Only One Field Mapping
    //
    {
        ModelA ma;
        ModelD md;
        auto field = FieldExtractor { ma, md };

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

        assert (mapper.Query (ModelD {})
            .Aggregate (Count ()).Value () == countExpected);
        assert (mapper.Query (ModelD {})
            .LeftJoin (ModelA {}, field (ma.a_int) == field (md.d_int))
            .Aggregate (Count ()).Value () == countExpected);
        mapper.Insert (ModelA {}, false);
        assert (mapper.Query (ModelD {}).Select (field (md.d_int))
            .Union (mapper.Query (ModelA {}).Select (field (ma.a_int)))
            .ToList ().size () == countExpected + 1);
        assert (mapper.Query (ModelD {})
            .ToVector ()[countExpected - 1].d_int == lastIdExpected);
        auto firstTuple = mapper.Query (ModelD {})
            .Select (field (md.d_int))
            .ToList ().front ();
        assert (std::get<0> (firstTuple).Value () == firstIdExpected);
    }

    //
    // Case: Scope of Mapper
    //
    {
        Queryable<ModelA> *queryable;
        {
            ORMapper mapper2 (TESTDB);
            queryable = new Queryable<ModelA> { mapper2.Query (ModelA {}) };
        }
        assert (queryable->ToList ().size () == 1);
        delete queryable;
    }

    //
    // Case: Not Code First Cases
    //
    {
        mapper.DropTbl (ModelD {});
        {
            sqlite3 *db;
            sqlite3_open (TESTDB, &db);
            sqlite3_exec (
                db,
                "CREATE TABLE ModelD (d_int INTEGER,d_str TEXT);"
                "INSERT INTO ModelD values (1, 'John');",
                0, 0, 0);
            sqlite3_close (db);

            std::string exMsg;
            try
            {
                mapper.Query (ModelD {}).ToList ();
            }
            catch (const std::exception &ex)
            {
                exMsg = ex.what ();
            }
            assert (exMsg == "SQL error: 'Bad Column Count' at 'select * from ModelD;'");
        }
        mapper.DropTbl (ModelD {});
        mapper.CreateTbl (ModelD {});
    }

    //
    // Case: Chinese Chars
    //
    {
        mapper.DropTbl (ModelA {});
        mapper.CreateTbl (ModelA {});
        mapper.Insert (ModelA { 0, "你好", 3.14, nullptr, nullptr, nullptr }, false);
        mapper.Insert (ModelA { 0, u8"世界", 7.28, nullptr, nullptr, nullptr }, false);
        auto chinese = mapper.Query (ModelA {}).ToVector ();
        assert (chinese[0].a_string == "你好" &&
            chinese[1].a_string == u8"世界");
    }

    std::cout << "Done in " << time (nullptr) - begTime << " seconds" << std::endl;
    return 0;
}
