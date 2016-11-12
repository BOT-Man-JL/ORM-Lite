# ORM Lite

**ORM Lite** is a C++ [_**Object Relation Mapping** (ORM)_](https://en.wikipedia.org/wiki/Object-relational_mapping) for **SQLite3**,
written in Modern C++ style.

## Features

- Easy to Use
- Light Weight
- Compile-time Overhead
- Fluent Interface

## Usage

### [View Full Documents](docs/ORM-Lite-doc.md)

### Including *ORM Lite*

Before we start,
Include `ORMLite.h` and `sqlite3.h`/`sqlite3.c` into your Project;

``` C++
#include "ORMLite.h"
using namespace BOT_ORM;

struct MyClass
{
    int id;
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

// Define a Query Helper Object
MyClass helper;

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
auto query0 = mapper.Query (helper).ToVector ();
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
for (int i = 50; i < 100; i++)
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
// Select by Query :-)
auto query1 = mapper.Query (helper)    // Link 'helper' to its fields
    .Where (
        Field (helper.name) == "July" &&
        (Field (helper.id) <= 90 && Field (helper.id) >= 60)
    )
    .OrderBy (helper.id, true)
    .Limit (3, 10)
    .ToVector ();

// Remarks:
// sql = SELECT * FROM MyClass
//       WHERE (name='July' and (id<=90 and id>=60))
//       ORDER BY id DESC
//       LIMIT 3 OFFSET 10
// query1 =
// [{ 80, 17.0, "July"}, { 79, 16.8, "July"}, { 78, 16.6, "July"}]

// Count by Query :-)
auto count = mapper.Query (helper)    // Link 'helper' to its fields
    .Where (Field (helper.name) == "July")
    .Count ();

// Remarks:
// sql = SELECT COUNT (*) AS NUM FROM MyClass WHERE (name='July')
// count = 50

// Delete by Query :-)
mapper.Query (helper)                  // Link 'helper' to its fields
    .Where (helper.name = "July")      // Trick ;-)
    .Delete ();

// Remarks:
// sql = DELETE FROM MyClass WHERE (name='July')
```

## Implementation Details

- Using `std::tuple` ~~Visitor Pattern~~ to Visit the Object;
- Using **Macro** `#define (...)` to Generate Codes;
- Using `std::stringstream` to **(De)serialization** data;
- Using **Self-Refrence** to Implement *Fluent Interface*
