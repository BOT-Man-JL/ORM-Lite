# ORM Lite

## Requirement

- **C++ 14** Support
  - MSVC >= 14 (2015)
  - gcc >= 5
- **SQLite 3** (zipped in *src*)

## `BOT_ORM`

``` cpp
#include "ORMLite.h"
using namespace BOT_ORM;
using namespace BOT_ORM::Expression;
using namespace BOT_ORM::Helper;
```

Modules under `namespace BOT_ORM`:

- `BOT_ORM::Nullable`
- `BOT_ORM::ORMapper`

Modules under `BOT_ORM::ORMapper`:

- `BOT_ORM::ORMapper::Queryable<...>`
- `BOT_ORM::ORMapper::FieldExtractor`

Modules under `namespace BOT_ORM::Helper`:

- `BOT_ORM::Helper::FieldExtractor ()`
- `BOT_ORM::Helper::Count ()`
- `BOT_ORM::Helper::Sum ()`
- `BOT_ORM::Helper::Avg ()`
- `BOT_ORM::Helper::Max ()`
- `BOT_ORM::Helper::Min ()`

Modules under `namespace BOT_ORM::Expression`:

- `BOT_ORM::Expression::Comparable`
- `BOT_ORM::Expression::Field`
- `BOT_ORM::Expression::NullableField`
- `BOT_ORM::Expression::AggregateFunc`
- `BOT_ORM::Expression::Expr`
- `BOT_ORM::Expression::SetExpr`

## `BOT_ORM::Nullable`

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

## `BOT_ORM::ORMapper`

### Inject `BOT_ORM::ORMapper` into Class

``` cpp
struct MyClass
{
    int field1;
    double field2;
    std::string field3;

    Nullable<int> field4;
    Nullable<double> field5;
    Nullable<std::string> field6;

    // Inject ORM-Lite into this Class :-)
    ORMAP ("TableName", field1, field2, field3,
           field4, field5, field6);
};
```

In this Sample, `ORMAP ("TableName", ...)` do that:
- Class `MyClass` will be mapped into Table `TableName`;
- Not `Nullable` members will be mapped as `NOT NULL`;
- `field1, field2, field3, field4, field5, field6` will be mapped
  into `INT field1 NOT NULL`, `REAL field2 NOT NULL`,
  `TEXT field3 NOT NULL`, `INT field4`, `REAL field5`
  and `TEXT field6` respectively;
- The first entry `field1` will be set as the **Primary Key** of the Table;

Note that:
- Currently Only Support
  - T such that `std::is_integral<T>::value == true`
    and **NOT** `char` or `*char_t`
  - T such that `std::is_floating_point<T>::value == true`
  - T such that `std::is_same<T, std::string>::value == true`
  - which are mapped as `INTEGER`, `REAL` and `TEXT` (SQLite3);
- Field Names MUST **NOT** be SQL Keywords (SQL Constraint);
- `std::string` Value MUST **NOT** contain `\0` (SQL Constraint);
- `ORMAP (...)` will **auto** Inject some **private members**;
  - `__Accept ()` to Implement **Visitor Pattern**;
  - `__Tuple ()` to **Flatten** data to tuple;
  - `__FieldNames ()` and `__TableName` to store strings;

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