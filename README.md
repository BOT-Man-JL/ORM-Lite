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

``` cpp
#include "ORMLite.h"
using namespace BOT_ORM;

struct MyClass
{
    int id;
    double score;
    std::string name;

    Nullable<int> age;
    Nullable<double> salary;
    Nullable<std::string> title;

    // Inject ORM-Lite into this Class :-)
    ORMAP (MyClass, id, score, name, age, salary, title);
};
```

`Nullable<T>` helps us construct `Nullable` Value in C++,
which is described in the [Document](docs/ORM-Lite-doc.md) 😁

In this Sample, `ORMAP (MyClass, ...)` do that:
- `Class MyClass` will be mapped into `TABLE MyClass`;
- Not `Nullable` members will be mapped as `NOT NULL`;
- `id, score, name, age, salary, title` will be mapped into 
  `INT id NOT NULL`, `REAL score NOT NULL`, `TEXT name NOT NULL`,
  `INT age`, `REAL salary` and `TEXT title` respectively;
- The first entry `id` will be set as the **Primary Key** of the Table;

### Create or Drop a Table for the Class

``` cpp
// Open a Connection with *test.db*
ORMapper mapper ("test.db");

// Create a table for "MyClass"
mapper.CreateTbl (MyClass {});

// Drop the table "MyClass"
mapper.DropTbl (MyClass {});
```

| id| score| name|    age|  salary|  title|
|---|------|-----|-------|--------|-------|
|  0|   0.2| John|     21|  `null`| `null`|
|  1|   0.4| Jack| `null`|    3.14| `null`|
|  2|   0.6| Jess| `null`|  `null`|    Dr.|
|...|   ...|  ...|    ...|     ...|    ...|

### Working on *Database* with *ORMapper*

#### Basic Usage

``` cpp
std::vector<MyClass> initObjs =
{
    { 0, 0.2, "John" },
    { 1, 0.4, "Jack" },
    { 2, 0.6, "Jess" }
};

// Insert Values into the table
for (const auto obj : initObjs)
    mapper.Insert (obj);

// Update Entry by KEY (id)
initObjs[1].salary = nullptr;
initObjs[1].title = "St.";
mapper.Update (initObjs[1]);

// Delete Entry by KEY (id)
mapper.Delete (initObjs[2]);

// Transactional Statements
try
{
    mapper.Transaction ([&] ()
    {
        mapper.Delete (initObjs[0]);
        mapper.Insert (MyClass { 1, 0, "Joke" });
    });
}
catch (const std::exception &ex)
{
    // If any statement Failed, throw an exception
    // "SQL error: UNIQUE constraint failed: MyClass.id"

    // Remarks:
    // mapper.Delete (initObjs[0]); will not applied :-)
}

// Select All to Vector
auto result1 = mapper.Query (MyClass {}).ToVector ();
// result1 = [{ 0, 0.2, "John", 21,   null, null  },
//            { 1, 0.4, "Jack", null, null, "St." }]
```

#### Batch Operations

``` cpp
// Insert by Batch Insert
// Performance is much Better than Separated Insert :-)
std::vector<MyClass> dataToSeed;
for (int i = 50; i < 100; i++)
    dataToSeed.emplace_back (MyClass { i, i * 0.2, "July" });
mapper.Transaction ([&] ()
{
    mapper.InsertRange (dataToSeed);
});

// Update by Batch Update
for (size_t i = 0; i < 50; i++)
    dataToSeed[i].score += 1;
mapper.Transaction ([&] ()
{
    mapper.UpdateRange (dataToSeed);
});
```

#### Composite Query

``` cpp
// Define a Query Helper Object
MyClass helper;

// Select by Query :-)
auto result2 = mapper.Query (helper)   // Link 'helper' to its fields
    .Where (
        Field (helper.name) == "July" &&
        (Field (helper.id) <= 90 && Field (helper.id) >= 60)
    )
    .OrderByDescending (helper.id)
    .Take (3)
    .Skip (10)
    .ToVector ();

// Remarks:
// sql = SELECT * FROM MyClass
//       WHERE (name='July' and (id<=90 and id>=60))
//       ORDER BY id DESC
//       LIMIT 3 OFFSET 10
// result2 =
// [{ 80, 17.0, "July", null, null, null },
//  { 79, 16.8, "July", null, null, null },
//  { 78, 16.6, "July", null, null, null }]

// Reusable Query Object :-)
auto query = mapper.Query (helper)     // Link 'helper' to its fields
    .Where (Field (helper.name) == "July");

// Aggregate Function by Query :-)
auto count = query.Count ();

// Remarks:
// sql = SELECT COUNT (*) FROM MyClass WHERE (name='July')
// count = 50

// Update by Query :-)
query.Update (helper.score = 10, helper.name = "Jully");

// Remarks:
// sql = UPDATE MyClass SET score=10,name='Jully' WHERE (name='July')

// Delete by Query :-)
mapper.Query (helper)                  // Link 'helper' to its fields
    .Where (helper.name = "Jully")     // Trick ;-) (Detailed in Docs)
    .Delete ();

// Remarks:
// sql = DELETE FROM MyClass WHERE (name='Jully')
```

## Implementation Details

- Using **Visitor Pattern** to Traverse the Entity;
- Using **Macro** `#define (...)` to Generate Codes;
- Using `std::stringstream` to **(De)serialization** data;
- Using **Self Refrence** to Implement *Fluent Interface*

Details of the Design in **Chinese** are posted on my
[Blog](https://BOT-Man-JL.github.io/articles/#2016/How-to-Design-a-Naive-Cpp-ORM) 😉