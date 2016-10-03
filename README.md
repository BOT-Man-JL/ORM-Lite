# ORM Lite

**ORM Lite** is a C++ [_**Object Relation Mapping** (ORM)_](https://en.wikipedia.org/wiki/Object-relational_mapping) for **SQLite3**,
written in C++ 11 style.

## Features

- Easy to Use
- Light Weight
- High Performance

## Basic Usage

### Step 1

Include `ORMLite.h` and `sqlite3.h`/`sqlite3.c` into your Project;

Add `ORMAP (MyClass, ...)` into your Mapped Class/Struct;

``` C++
#include "ORMLite.h"
using namespace BOT_ORM;

class MyClass
{
    ORMAP (MyClass, id, real, str)
public:
    long id;
    double real;
    std::string str;
};
```

For example, `ORMAP (MyClass, id, real, str)` means that:
- `Class MyClass` will be mapped into `TABLE MyClass`;
- `int id`, `double real` and `std::string str` will be mapped
  into `INT id`, `REAL real` and `TEXT str` respectively;
- The first item `id` will be set as the **Primary Key** of the Table;

### Step 2

New a `ORMapper` to Handle the Ops;

``` C++
// Store the Data in "test.db"
ORMapper<MyClass> mapper ("test.db");

// Create a table for "MyClass"
mapper.CreateTbl ();

// Insert Values into the table
mapper.Insert (MyClass { 1, 0.2, "John" });
mapper.Insert (MyClass { 2, 0.4, "Jack" });
mapper.Insert (MyClass { 3, 0.6, "Jess" });

// Update the Value by its KEY (id)
mapper.Update (MyClass { 2, 0.6, "Jack" });

// Delete the Value by its KEY (id)
mapper.Delete (MyClass { 3, 0.6, "Jess" });

// Query the Entries in the table, and Set to entries
std::vector<MyClass> entries;
mapper.Query (entries);  // entries = [MyClass { 1, 0.2, "John" },
                         //            MyClass { 2, 0.6, "Jack" }]

// Get the Count of Entries in the table
auto count = mapper.Count ();  // count = 2

// Drop the table "MyClass"
//mapper.DropTbl ();

// View the latest Error Message
//auto errStr = mapper.ErrMsg ();
```

And the final table **MyClass** in **test.db** will be:

| id | real |  str |
|------------------|
|  1 |  0.2 | John |
|  2 |  0.6 | Jack |

## Constraints

- Hooked Class/Struct Must have a **Default Constructor**
  (POD is Better :relieved:)
- Currently Only Support `long`, `double` and `std::string` Types,
  which are stored in DB as `INTEGER`, `REAL` and `TEXT` (**SQLite3**)
- If you Attempt to Use other Types, you will see the Error:
  `Only Support long, double, std::string :-(`
- Only Support Basic [**CRUD**](https://en.wikipedia.org/wiki/Create,_read,_update_and_delete) as described above...

## Implementation Details

- Using **Visitor Pattern** to *Hook* into the wanted Class/Struct;
- Using **Template** to Generate Visitors in *Compile Time*;
- Using **Variadic Template** to Fit into *Various Types*;
- Using **Macro** `#define (...)` to Generate Hook Codes;
- Using **Serialization** and **Deserialization** to *Interchange Data*;
