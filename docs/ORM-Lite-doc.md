# ORM Lite

## Include to Your Project

``` C++
#include "ORMLite.h"
using namespace BOT_ORM;
```

Remarks:
- Remember to Build `sqlite3.c` in your Project;
- And `sqlite3.c` has been zipped
  (because of its large size :sweat_smile:)

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

Execute `CREATE TABLE MyClass (...);`

### bool DropTbl ()

Drop Table `MyClass` from the DB File;

Execute `DROP TABLE MyClass;`

### bool Insert (const MyClass &value)

Insert `value` into Table `MyClass`;

Execute `INSERT INTO MyClass VALUES (...);`

### bool Insert (const T\<MyClass\> &values)

Insert `values` into Table `MyClass`;

`values` must **SUPPORT** `forward_iterator`;
    
Execute `INSERT INTO MyClass VALUES (...), (...) ...;`

### bool Delete (const MyClass &value)

Delete Entry `value` in Table `MyClass`
with the Same `KEY` with variable `value`;

Execute `DELETE FROM MyClass WHERE` `KEY` `=` `value.id` `;`

### bool Delete (const std::string &sqlStr = "")

Execute `DELETE FROM MyClass` `sqlStr;`

### bool Update (const MyClass &value)

Update Entry `value` in Table `MyClass`
with the Same `KEY` with variable `value`;

Execute `UPDATE MyClass SET (...) WHERE` `KEY` `=` `value.id` `;`

### bool Update (const T\<MyClass\> &values)
    
Update Entries with the Same `KEY` with `values`;

`values` must **SUPPORT** `forward_iterator`;

Execute Multiple `UPDATE MyClass SET (...) WHERE` `KEY` `=` `value.id` `;`

### bool Select (T\<MyClass\> &out, const std::string &sqlStr = "")

Set the Select into Container `out`;

`out` must **SUPPORT** `push_back (const MyClass &)` Function;

Execute `SELECT * FROM MyClass` `sqlStr`";

### long Count (const std::string &sqlStr = "")

Execute `SELECT COUNT(*) FROM MyClass` `sqlStr`",

Return:
- Count of entries fit `sqlStr` in `MyClass`;
- -1 if **Query Error**;

### ORQuery Query (const MyClass &queryHelper)

Return new `ORQuery` object and Capturing `queryHelper`;

Details in `## ORQuery` Section;

### Note that

Only a **bool returning** Function Succeeded,
it would return `true`; otherwise, return `false`;

## ORQuery

### ORQuery &Where (const T &property, const std::string &relOp = "=", T value = property)

Generate `WHERE` String: `field name of (property)` `relOp` `value`;

Remarks:
- `Where (property)` is short for `Where (property, "=", property)`
- If `property` is not a member of `queryHelper`,
  throw `std::runtime_error`;

### ORQuery &WhereLBracket () / WhereRBracket () / WhereAnd () / WhereOr ()

Generate `WHERE` String: `(` / `)` / `and` / `or`;

### ORQuery &OrderBy (const T &property, bool isDecreasing = false)

Generate `ORDER BY` `field name of (property)` or `ORDER BY` `field name of (property)` `DESC` (if isDecrease);

Remarks:
- If `property` is not a member of `queryHelper`,
  throw `std::runtime_error`;

### ORQuery &Limit (size_t count, size_t offset = 0)

Generate `LIMIT` `count` `OFFSET` `offset`;

### std::string GetSQL () const

Get the Generated SQL String of this Query;

And you can Use this String to Call `Select`, `Count` and `Delete`;

### std::vector\<MyClass\> ToVector () / std::list\<MyClass\> ToList ()

Retrieve Select Result under **Constraints**;

Execute `SELECT * FROM MyClass WHERE ... ORDER BY ... LIMIT ...;`

### long Count ()

Return:
- Return Count under **_WHERE_ Constraints**;
- -1 if **Query Error**;

Execute `SELECT COUNT (*) FROM MyClass WHERE ... ;`

### bool Delete ()

Delete Entries under **_WHERE_ Constraints**;

Execute `DELETE FROM MyClass WHERE ... ;`

Return:
- `true` if Succeeded;
- `false` otherwise;
