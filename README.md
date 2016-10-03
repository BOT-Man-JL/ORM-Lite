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

### Handle with a *ORMapper*

``` C++
// Store the Data in "test.db"
ORMapper<MyClass> mapper ("test.db");

// Create a table for "MyClass"
mapper.CreateTbl ();

// Insert Values into the table
mapper.Insert (MyClass { 1, 0.2, "John" });
mapper.Insert (MyClass { 2, 0.4, "Jack" });
mapper.Insert (MyClass { 3, 0.6, "Jess" });
mapper.Insert (MyClass { 4, 0.8, "July" });
mapper.Insert (MyClass { 5, 1.0, "July" });

// Update Entry by KEY (id)
mapper.Update (MyClass { 2, 0.6, "Jack" });

// Delete Entry by KEY (id)
mapper.Delete (MyClass { 3, 0.6, "Jess" });

// Query Entries
std::vector<MyClass> query1;
mapper.Query (query1);
// query1 = [MyClass { 1, 0.2, "John" },
//           MyClass { 2, 0.6, "Jack" },
//           MyClass { 4, 0.8, "July" },
//           MyClass { 5, 1.0, "July" }]

// Count Entries
auto count1 = mapper.Count ();  // count = 4

// Query Entries by Condition
std::vector<MyClass> query2;
mapper.Query (query2, "where str='July' order by real desc");
// query2 = [MyClass { 5, 1.0, "July" },
//           MyClass { 4, 0.8, "July" }]

// Delete Entries by Condition
mapper.Delete ("where str='July'");

// Count Entries by Condition
auto count2 = mapper.Count ("where str='July'");  // count = 0

// View the latest Error Message
mapper.Insert (MyClass { 1, 0, "Admin" });
auto errStr = mapper.ErrMsg ();
// errStr = "SQL error: UNIQUE constraint failed: MyClass.id"

// Drop the table "MyClass"
mapper.DropTbl ();
```

**MyClass** Format in **test.db**:

| id| real|  str|
|---|-----|-----|
|  1|  0.2| John|
|...|  ...|  ...|

_(Note that: Wrap **String** with **''** in **SQL**)_ :joy:

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
