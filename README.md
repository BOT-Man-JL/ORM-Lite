# ORM Lite

**ORM Lite** is a C++ [_**Object Relation Mapping** (ORM)_](https://en.wikipedia.org/wiki/Object-relational_mapping) for **SQLite3**,
written in C++ 11 style.

## Features

- Easy to Use
- Light Weight
- High Performance

## Usage

Before we start,
Include `ORMLite.h` and `sqlite3.h`/`sqlite3.c` into your Project;

### Include *ORM Lite*

``` C++
#include "ORMLite.h"
using namespace BOT_ORM;

class MyClass
{
    // Inject ORM-Lite into this Class
    ORMAP (MyClass, id, real, str)
public:
    long id;
    double real;
    std::string str;
};
```

In this Sample, `ORMAP (MyClass, id, real, str)` means that:
- `Class MyClass` will be mapped into `TABLE MyClass`;
- `int id`, `double real` and `std::string str` will be mapped
  into `INT id`, `REAL real` and `TEXT str` respectively;
- The first item `id` will be set as the **Primary Key** of the Table;

_(Note that: **No Semicolon ';'** after the **ORMAP**)_ :wink:

### Handle *db* with a *ORMapper*

#### Basic Usage

``` C++
// Store the Data in "test.db"
ORMapper<MyClass> mapper ("test.db");

// Create a table for "MyClass"
mapper.CreateTbl ();

// Insert Values into the table
std::vector<MyClass> initObjs =
{
    { 0, 0.2, "John" },
    { 1, 0.4, "Jack" },
    { 2, 0.6, "Jess" }
};
for (auto obj : initObjs)
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

**MyClass** in **test.db**:

| id| real|  str|
|---|-----|-----|
|  1|  0.2| John|
|...|  ...|  ...|

#### Complex Usage :-)

``` C++
// ReSeed data :-)
for (long i = 50; i < 100; i++)
    mapper.Insert (MyClass { i, i * 0.2, "July" });

// Define a Query Helper Object
MyClass _mc;

// Select by Query
auto query1 = mapper.Query (_mc)    // Link '_mc' to its fields
    .Where (_mc.name, "=", "July")
    .WhereAnd ()
    .WhereBracket (true)
        .Where (_mc.id, "<=", 90)
        .WhereAnd ()
        .Where (_mc.id, ">=", 60)
    .WhereBracket (false)
    .OrderBy (_mc.id, true)
    .Limit (3, 10)
    .ToVector ();

// Select by SQL
std::vector<MyClass> query2;
mapper.Select (query2,
               "where (name='July' and (id<=90 and id>=50))"
               " order by id desc"
               " limit 3 offset 10");

// Note that: query1 = query2 =
// [{ 80, 16.0, "July"}, { 79, 15.8, "July"}, { 78, 15.6, "July"}]

// Count by Query
auto count1 = mapper.Query (_mc)    // Link '_mc' to its fields
    .Where (_mc.name, "=", "July")
    .Count ();

// Count by SQL
auto count2 = mapper.Count ("where (name='July')");

// Note that:
// count1 = count2 = 50

// Delete by Query
mapper.Query (_mc)                  // Link '_mc' to its fields
    .Where (_mc.name, "=", "July")
    .Delete ();

// Delete by SQL
mapper.Delete ("where (name='July')");
```


## Constraints

- Hooked Class MUST have **Default Constructor**
- Currently Only Support `long`, `double` and `std::string` Types,
  which are stored in DB as `INTEGER`, `REAL` and `TEXT` (**SQLite3**)
- If you Attempt to Use other Types, you will see the Compile Error:
  `Only Support long, double, std::string :-(`
- Currently Only Support Basic [**CRUD**](https://en.wikipedia.org/wiki/Create,_read,_update_and_delete) as described above...

## Implementation Details

- Using **Visitor Pattern** to *Hook* into the wanted Class/Struct;
- Using **Template** to Generate Visitors in *Compile Time*;
- Using **Variadic Template** to Fit into *Various Types*;
- Using **Macro** `#define (...)` to Generate Hook Codes;
- Using **Serialization** and **Deserialization** to *Interchange Data*;
