# ORM Lite

## Include to Your Project

``` C++
#include "ORMLite.h"
using namespace BOT_ORM;
```

Remarks:
- Remember to Build `sqlite3.c` in your Project

## Hook Class

``` C++
#include "ORMLite.h"
using namespace BOT_ORM;

class MyClass
{
    // Inject ORM-Lite into this Class
    ORMAP (MyClass, id, score, name)
public:
    long id;
    double score;
    std::string name;
};
```

In this Sample, `ORMAP (MyClass, id, score, name)` means that:
- `Class MyClass` will be mapped into `TABLE MyClass`;
- `int id`, `double score` and `std::string name` will be mapped
  into `INT id`, `REAL score` and `TEXT name` respectively;
- The first item `id` will be set as the **Primary Key** of the Table;

Note that:
- Hooked Class MUST have **Default Constructor**
- Currently Only Support `long`, `double` and `std::string` Types,
  which are stored in DB as `INTEGER`, `REAL` and `TEXT` (**SQLite3**)
- If you Attempt to Use other Types, you will see the Compile Error:
  `Only Support long, double, std::string :-(`

## ORMapper

### ORMapper\<MyClass\> (const std::string &dbName)

Construct a **O/R Mapper** in file `dbName`;

All operations on `MyClass` will be mapped into `dbName`;

### const std::string &ErrMsg () const

Return the latest Error Message;

### bool CreateTbl ()

Create Table `MyClass` for class `MyClass`;

Excute `CREATE TABLE MyClass (...);`

### bool DropTbl ()

Drop Table `MyClass` from the DB File;

Excute `DROP TABLE MyClass;`

### bool Insert (MyClass &value)

Insert `value` into Table `MyClass`;

Excute `INSERT INTO MyClass VALUES (...);`

### bool Delete (const std::string &sqlStr = "")

Excute "`DELETE FROM MyClass` `sqlStr;`

### bool Delete (MyClass &value)

Delete Entry `value` in Table `MyClass`
with the Same `KEY` with variable `value`;

Excute "`DELETE FROM MyClass WHERE` `KEY` `=` `value.id` `;`

### bool Update (MyClass &value)

Update Entry `value` in Table `MyClass`
with the Same `KEY` with variable `value`;

Excute "`UPDATE MyClass SET (...) WHERE` `KEY` `=` `value.id` `;`

### bool Select (T<MyClass> &out, const std::string &sqlStr = "")

Excute "`SELECT * FROM MyClass` `sqlStr`",
and Set the result into `out`;

### size_t Count (const std::string &sqlStr = "")

Excute "`SELECT COUNT(*) FROM MyClass` `sqlStr`",

Return:
- Count of entries fit `sqlStr` in `MyClass`;
- 0 if **No entry Found** or **Query Error**

### ORQuery Query (C &qObj)

Return new `ORQuery` object and Capturing `qObj`;

### Note that

Only a **bool returning** Function Succeeded,
it would return `true`; otherwise, return `false`;

## ORQuery

### ORQuery &Where (T &property, const std::string &relOp, T value)

Generate `WHERE` String: `field name of (property)` `relOp` `value`;

### ORQuery &WhereBracket (bool isLeft)

Generate `WHERE` String: `(` **(isLeft == true)** or `)` **(isLeft == false)**;

### ORQuery &WhereAnd () / WhereOr ()

Generate `WHERE` String: `and` / `or`;

### ORQuery &OrderBy (T &property, bool isDecreasing = false)

Generate `ORDER BY` `field name of (property)` or `ORDER BY` `field name of (property)` `DESC`;

### ORQuery &Limit (size_t count, size_t offset = 0)

Generate `LIMIT` `count` `OFFSET` `offset`;

### std::vector\<C\> ToVector () / std::list\<C\> ToList ()

Retrieve Select Result under **Constraints**;

### size_t Count ()

Return Count under **Constraints**;

### bool Delete ()

Delete Entries under **Constraints**;