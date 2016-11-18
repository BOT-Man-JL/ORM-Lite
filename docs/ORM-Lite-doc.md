# ORM Lite

## Requirement

- **C++ 14** Support
  - MSVC >= 14 (2015)
  - gcc >= 5
- **SQLite 3** (zipped in *src*)

## Include to Your Project

``` C++
#include "ORMLite.h"
using namespace BOT_ORM;
```

## Inject Class

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

Note that:
- `ORMAP (...)` will **auto** Inject some **private members**
  , but **NO damage** to the Class :wink:
- Currently Only Support
  - T such that `std::is_integral<T>::value == true` and Not **Char**
  - T such that `std::is_floating_point<T>::value == true`
  - T such that `std::is_same<T, std::string>::value == true`
  - which are mapped as `INTEGER`, `REAL` and `TEXT` (SQLite3);
- Field Names MUST **NOT** be SQL Keywords;
- `std::string` Value MUST **NOT** contain `\0` (SQLite3 Constraint);

## ORMapper

### ORMapper (const string &connectionString)

Construct a **O/R Mapper** to connect to `connectionString`;

### void CreateTbl (const MyClass &entity)

Create Table `MyClass` for class `MyClass`;

Execute `CREATE TABLE MyClass (...);`

### void DropTbl (const MyClass &)

Drop Table `MyClass` from the DB File;

Execute `DROP TABLE MyClass;`

### void Insert (const MyClass &entity)

Insert `entity` into Table `MyClass`;

Execute `INSERT INTO MyClass VALUES (...);`

### void InsertRange (const Container\<MyClass\> &entities)

Insert `entities` into Table `MyClass`;

`entities` must **SUPPORT** `forward_iterator`;
    
Execute `INSERT INTO MyClass VALUES (...), (...) ...;`

### void Update (const MyClass &entity)

Update Entity in Table `MyClass` with the Same `KEY` with `entity`;

Execute `UPDATE MyClass SET (...) WHERE` `KEY` `=` `entity.id` `;`

### void UpdateRange (const Container\<MyClass\> &entities)
    
Update Entries with the Same `KEY` with `entities`;

`entities` must **SUPPORT** `forward_iterator`;

Execute Multiple `UPDATE MyClass SET (...) WHERE` `KEY` `=` `entity.id` `;`

### void Delete (const MyClass &entity)

Delete Entry in Table `MyClass` with the Same `KEY` with `entity`;

Execute `DELETE FROM MyClass WHERE` `KEY` `=` `entity.id` `;`

### ORQuery\<MyClass\> Query (const MyClass &queryHelper)

Return new `ORQuery` object and Capturing `queryHelper`;

Details in `## ORQuery` Section;

## ORQuery

### ORQuery &Where (const Expr &expr)

Generate `WHERE (` `expr` `)`;

Details in `## Expr` Section;

### ORQuery &OrderBy (const T &property)

Generate `ORDER BY` `field_name_of<property>`;

If `property` is not a member of `queryHelper`,
throw `std::runtime_error`;

### ORQuery &OrderByDescending (const T &property)

Generate `ORDER BY` `field_name_of<property>` `DESC`;

If `property` is not a member of `queryHelper`,
throw `std::runtime_error`;

### ORQuery &Take (size_t count)

Generate `LIMIT` `count`;

### ORQuery &Skip (size_t count)

Generate `OFFSET` `count`;

### vector\<MyClass\> ToVector () / list\<MyClass\> ToList ()

Retrieve Select Result under **_WHERE_ Constraints**;

Execute `SELECT * FROM MyClass WHERE ... ORDER BY ... LIMIT ...;`

Remarks:
- `MyClass` **MUST** be **Copy Constructible**;
- Deserialize Value to a copy of `queryHelper`;
- Copy the copy to `std::vector` / `std::list`;

### void Delete ()

Delete Entries under **_WHERE_ Constraints**;

Execute `DELETE FROM MyClass WHERE ... ORDER BY ... LIMIT ...;`

### unsigned long long Count ()

Return the `Count` under **_WHERE_ Constraints**;

Execute `SELECT COUNT (*) FROM MyClass WHERE ... ORDER BY ... LIMIT ...;`

### T Sum / Avg / Max / Min (const T &property)

Return the `Sum / Average / Max / Min` under **_WHERE_ Constraints**;
    
Execute `SELECT SUM/AVG/MAX/MIN (field_name_of<property>) FROM MyClass WHERE ... ORDER BY ... LIMIT ...;`

If `property` is not a member of `queryHelper`,
throw `std::runtime_error`;

## Expr

### Expr (const T &property, const string &relOp = "=", T value = property)

Generate `field_name_of<property>` `relOp` `value`;

Remarks:
- `Expr (property)` is short for `Expr (property, "=", property)`
- If `property` is not a member of `queryHelper`,
  throw `std::runtime_error` at Calling `Where`;

### Expr operator && / || (const Expr &left, const Expr &right)

Generate `(` `left` `&& / ||` `right` `)`;

### Expr_Field\<T\> (const T &property) (*Helper Struct of Expr*)

#### Expr operator *OP* (T value)

Return `Expr { property, OP_STR, value }; }`;

| OP |  OP_STR  |
|----|----------|
| == |    "="   |
| != |   "!="   |
|  > |    ">"   |
| >= |   ">="   |
|  < |    "<"   |
| <= |   "<="   |

### Expr_Field<T> Field (T &property)

Return `Expr_Field<T> { property }`;

## Error Handling

All Functions will throw `std::runtime_error`
with the **Error Message** if Failed at Query;