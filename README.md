# ORM Lite

**ORM Lite** is a C++ [_**Object Relation Mapping** (ORM)_](https://en.wikipedia.org/wiki/Object-relational_mapping) for **SQLite3**,
written in C++ 11 style.

## Features

- Easy to Use
- Light Weight
- High Performance

## Usage

### [View Full Documents](https://github.com/BOT-Man-JL/ORM-Lite/tree/master/docs/ORM-Lite-doc.md)

### Including *ORM Lite*

Before we start,
Include `ORMLite.h` and `sqlite3.h`/`sqlite3.c` into your Project;

``` C++
#include "ORMLite.h"
using namespace BOT_ORM;

struct MyClass
{
    long id;
    double score;
    std::string name;

    // Inject ORM-Lite into this Class
    ORMAP (MyClass, id, score, name)
};
```

In this Sample, `ORMAP (MyClass, id, score, name)` means that:
- `Class MyClass` will be mapped into `TABLE MyClass`;
- `int id`, `double score` and `std::string name` will be mapped
  into `INT id`, `REAL score` and `TEXT name` respectively;
- The first item `id` will be set as the **Primary Key** of the Table;

_(Note that: **No Semicolon ';'** after the **ORMAP**)_ :wink:

### Create or Drop a Table for this Class

``` C++
// Open a Connection with *test.db*
ORMapper<MyClass> mapper ("test.db");

// Create a table for "MyClass"
mapper.CreateTbl ();

// Drop the table "MyClass"
mapper.DropTbl ();
```

| id| score| name|
|---|------|-----|
|  1|   0.2| John|
|...|   ...|  ...|

### Working on *db* with *ORMapper*

#### Basic Usage

``` C++
std::vector<MyClass> initObjs =
{
    { 0, 0.2, "John" },
    { 1, 0.4, "Jack" },
    { 2, 0.6, "Jess" }
};

// Open a Connection with *test.db*
ORMapper<MyClass> mapper ("test.db");

// Insert Values into the table
for (const auto obj : initObjs)
    mapper.Insert (obj);

// Update Entry by KEY (id)
initObjs[1].score = 1.0;
mapper.Update (initObjs[1]);

// Delete Entry by KEY (id)
mapper.Delete (initObjs[2]);

// Select All to Vector
auto query0 = mapper.Query (MyClass ()).ToVector ();
// query0 = [{ 0, 0.2, "John"},
//           { 1, 1.0, "Jack"}]

// If 'Insert' Failed, Print the latest Error Message
if (!mapper.Insert (MyClass { 1, 0, "Joke" }))
    auto err = mapper.ErrMsg ();
// err = "SQL error: UNIQUE constraint failed: MyClass.id"
```

#### Batch Operations

``` C++
// Insert by Batch Insert
// Performance is much Better than Separated Insert :-)
std::vector<MyClass> dataToSeed;
for (long i = 50; i < 100; i++)
    dataToSeed.emplace_back (MyClass { i, i * 0.2, "July" });
mapper.Insert (dataToSeed);

// Update by Batch Update
// Performance is little Better than Separated Update :-(
for (size_t i = 0; i < 50; i++)
    dataToSeed[i].score += 1;
mapper.Update (dataToSeed);
```

#### Composite Query

``` C++
// Define a Query Helper Object
MyClass _mc;

// Select by Query :-)
auto query1 = mapper.Query (_mc)    // Link '_mc' to its fields
    .Where (
        Field (_mc.name) == "July" &&
        (Field (_mc.id) <= 90 && Field (_mc.id) >= 60)
    )
    .OrderBy (_mc.id, true)
    .Limit (3, 10)
    .ToVector ();

// Select by SQL, NOT Recommended :-(
std::vector<MyClass> query2;
mapper.Select (query2,
               "where (name='July' and (id<=90 and id>=60))"
               " order by id desc"
               " limit 3 offset 10");

// Note that: query1 = query2 =
// [{ 80, 17.0, "July"}, { 79, 16.8, "July"}, { 78, 16.6, "July"}]

// Count by Query :-)
auto count1 = mapper.Query (_mc)    // Link '_mc' to its fields
    .Where (Field (_mc.name) == "July")
    .Count ();

// Count by SQL, NOT Recommended :-(
auto count2 = mapper.Count ("where (name='July')");

// Note that:
// count1 = count2 = 50

// Delete by Query :-)
mapper.Query (_mc)                  // Link '_mc' to its fields
    .Where (_mc.name = "July")      // Trick ;-)
    .Delete ();

// Delete by SQL, NOT Recommended :-(
mapper.Delete ("where (name='July')");
```

## Implementation Details

- Using **Visitor Pattern** to *Hook* into the wanted Class/Struct;
- Using **Template** to Generate Visitors in *Compile Time*;
- Using **Variadic Template** to Fit in *Various Types*
  (Maybe refactor into `std::tuple` :confused:);
- Using **Macro** `#define (...)` to Generate Hook Codes;
- Using **Serialization** and **Deserialization** to *Interchange Data*;
