# ORM Lite

## Requirement

- **C++ 14** Support
  - MSVC >= 14 (2015)
  - gcc >= 5
- **SQLite 3** (zipped in *src*)

## Include to Your Project

``` cpp
#include "ORMLite.h"
using namespace BOT_ORM;
```

Modules under `namespace BOT_ORM`:

- `BOT_ORM::Nullable`
- `BOT_ORM::ORMapper`
- `BOT_ORM::Field`

## `Nullable`

It keeps the Similar Semantic as `Nullable<T>` as `C#`; and
[Reference Here](http://stackoverflow.com/questions/2537942/nullable-values-in-c/28811646#28811646)

### Construction & Assignment

- Default Constructed / `nullptr` Constructed / `nullptr` Assigned 
  object is `NULL` Valued;
- Value Constructed / Value Assigned object is `NOT NULL` Valued
- `Nullable` Objects are **Copyable** / **Movable**,
  and the *Destination* Value has the *Same Value* as the *Source*

``` cpp
// Default or Null Construction
Nullable ();
Nullable (nullptr_t);

// Null Assignment
const Nullable<T> & operator= (nullptr_t);

// Value Construction
Nullable (const T &value);

// Value Assignment
const Nullable<T> & operator= (const T &value);
```

### Get Value

Return the `Underlying Value` of the object,
which is **Valid** only if it's `NOT NULL`;

``` cpp
const T &Value (); const
```

### Comparison

Two Objects have the Same value only if their `Nullable` Construction:
- Both are `NULL`;
- Both are `NOT NULL` and have the Same `Underlying Value`;

``` cpp
bool operator==(const Nullable<T> &op1, const Nullable<T> &op2);
bool operator==(const Nullable<T> &op1, T &op2);
bool operator==(T &op1, const Nullable<T> &op2);
bool operator==(const Nullable<T> &op1, nullptr_t);
bool operator==(nullptr_t, const Nullable<T> &op2);
```

## Inject into Class

``` cpp
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

In this Sample, `ORMAP (MyClass, ...)` do that:
- `Class MyClass` will be mapped into `TABLE MyClass`;
- Not `Nullable` members will be mapped as `NOT NULL`;
- `id, score, name, age, salary, title` will be mapped into 
  `INT id NOT NULL`, `REAL score NOT NULL`, `TEXT name NOT NULL`,
  `INT age`, `REAL salary` and `TEXT title` respectively;
- The first entry `id` will be set as the **Primary Key** of the Table;

Note that:
- `ORMAP (...)` will **auto** Inject some **private members**;
- Currently Only Support
  - T such that `std::is_integral<T>::value == true`
    and **NOT** `char` or `*char_t`
  - T such that `std::is_floating_point<T>::value == true`
  - T such that `std::is_same<T, std::string>::value == true`
  - which are mapped as `INTEGER`, `REAL` and `TEXT` (SQLite3);
- Field Names MUST **NOT** be SQL Keywords (SQL Constraint);
- `std::string` Value MUST **NOT** contain `\0` (SQL Constraint);

## ORMapper

### ORMapper (const string &connectionString)

Construct a **O/R Mapper** to connect to `connectionString`;

### void Transaction (Fn fn)

Invoke `fn` **Transactionally**;

``` cpp
try
{
    _connector.Execute ("begin transaction;");
    fn ();
    _connector.Execute ("commit transaction;");
}
catch (...)
{
    _connector.Execute ("rollback transaction;");
    throw;
}
```

### void CreateTbl (const MyClass &entity)

Create Table `MyClass` for class `MyClass`;

``` sql
CREATE TABLE MyClass (...);
```

### void DropTbl (const MyClass &)

Drop Table `MyClass` from the DB File;

``` sql
DROP TABLE MyClass;
```

### void Insert (const MyClass &entity)

Insert `entity` into Table `MyClass`;

``` sql
INSERT INTO MyClass VALUES (...);
```

### void InsertRange (const Container\<MyClass\> &entities)

Insert `entities` into Table `MyClass`;

`entities` must **SUPPORT** `forward_iterator`;
    
``` sql
INSERT INTO MyClass VALUES (...), (...) ...;
```

### void Update (const MyClass &entity)

Update Entity in Table `MyClass` with the Same `KEY` with `entity`;

``` sql
UPDATE MyClass SET (...) WHERE KEY = <entity.id>;
```

### void UpdateRange (const Container\<MyClass\> &entities)
    
Update Entries with the Same `KEY` with `entities`;

`entities` must **SUPPORT** `forward_iterator`;

``` sql
UPDATE MyClass SET (...) WHERE KEY = <entity.id>;
UPDATE MyClass SET (...) WHERE KEY = <entity.id>;
...
```

### void Delete (const MyClass &entity)

Delete Entry in Table `MyClass` with the Same `KEY` with `entity`;

``` sql
DELETE FROM MyClass WHERE KEY = <entity.id>;
```

### ORQuery\<MyClass\> Query (const MyClass &queryHelper)

Return new `ORQuery` object and Capturing `queryHelper`;

Detailed in `## ORQuery` Section;

## ORQuery

### ORQuery &Where (const Expr &expr)

Generate `WHERE ( <expr> )`;

Details in `## Expr` Section;

### ORQuery &OrderBy (const T &property)

Generate `ORDER BY <property>`;

If `property` is not a member of `queryHelper`,
throw `std::runtime_error`;

### ORQuery &OrderByDescending (const T &property)

Generate `ORDER BY <property> DESC`;

If `property` is not a member of `queryHelper`,
throw `std::runtime_error`;

### ORQuery &Take (size_t count)

Generate `LIMIT count`;

### ORQuery &Skip (size_t count)

Generate `OFFSET count`;

Remarks:
- If there is Not `Take`, it will **FAILED**;

### vector\<MyClass\> ToVector () / list\<MyClass\> ToList ()

Retrieve Select Result under **_WHERE_ _ORDER BY_ _LIMIT_ Constraints**;

``` sql
SELECT * FROM MyClass WHERE ... ORDER BY ... LIMIT ...;
```

Remarks:
- `MyClass` **MUST** be **Copy Constructible**;
- Deserialize Value to a copy of `queryHelper`;
- Copy the copy to `std::vector` / `std::list`;

### void Update (const Args & ... fields)

Update `fields` under **_WHERE_ Constraints**;

``` sql
UPDATE MyClass SET ... WHERE ...;
```

### void Delete ()

Delete Entries under **_WHERE_ Constraints**;

``` sql
DELETE FROM MyClass WHERE ...;
```

### unsigned long long Count ()

Return the `Count` under **_WHERE_ _ORDER BY_ _LIMIT_ Constraints**;

``` sql
SELECT COUNT (*) FROM MyClass WHERE ... ORDER BY ... LIMIT ...;
```

### T Sum / Avg / Max / Min (const T &property)

Return the `Sum / Average / Max / Min`
under **_WHERE_ _ORDER BY_ _LIMIT_ Constraints**;
    
``` sql
SELECT SUM (<property>) FROM MyClass WHERE ... ORDER BY ... LIMIT ...;
SELECT AVG (<property>) FROM MyClass WHERE ... ORDER BY ... LIMIT ...;
SELECT MAX (<property>) FROM MyClass WHERE ... ORDER BY ... LIMIT ...;
SELECT MIN (<property>) FROM MyClass WHERE ... ORDER BY ... LIMIT ...;
```

If `property` is not a member of `queryHelper`,
throw `std::runtime_error`;

## Expr

### Expr (const T &property, const string &relOp = "=", T value = property) / Expr (const Nullable<T> &property, bool isNull)

Generate `<property> relOp <value>` or `<property> is [NOT] NULL`;

Remarks:
- `Expr (property)` is short for `Expr (property, "=", property)`
- If `property` is not a member of `queryHelper`,
  throw `std::runtime_error` at Calling `Where`;

### Expr operator && / || (const Expr &left, const Expr &right)

Generate `( <left> && <right> )` or `( <left> || <right> )`;

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