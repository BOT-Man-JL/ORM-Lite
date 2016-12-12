# ORM Lite

## Requirements

- **C++ 14** Support
  - MSVC >= 14 (VS 2015 Update 3)
  - gcc >= 5.4
  - Clang >= 3.8
- **SQLite 3** Dependency

## Mapping

|        SQL Concepts | C++ Concepts | Notes |
|---------------------|--------------|-------|
|    Table / Relation |       Object | Deduced by `ORMAP` |
| Tuple (in `SELECT`) | `std::tuple` | Deduced by `Join` / `Select` |
|              Column |        Field | Decuced by `Field` |

## `BOT_ORM`

``` cpp
#include "ORMLite.h"
using namespace BOT_ORM;
using namespace BOT_ORM::Expression;
```

Macro `ORMAP` in `ORMLite.h`

- `ORMAP (TableName, PrimaryKey, ...);`

Modules under `namespace BOT_ORM`

- `BOT_ORM::Nullable`
- `BOT_ORM::ORMapper`
- `BOT_ORM::Queryable<QueryResult>`
- `BOT_ORM::FieldExtractor`
- `BOT_ORM::Constraint`

Modules under `namespace BOT_ORM::Expression`

- `BOT_ORM::Expression::Selectable`
- `BOT_ORM::Expression::Field`
- `BOT_ORM::Expression::NullableField`
- `BOT_ORM::Expression::Aggregate`
- `BOT_ORM::Expression::Expr`
- `BOT_ORM::Expression::SetExpr`
- `BOT_ORM::Expression::Count ()`
- `BOT_ORM::Expression::Sum ()`
- `BOT_ORM::Expression::Avg ()`
- `BOT_ORM::Expression::Max ()`
- `BOT_ORM::Expression::Min ()`

Static Modules under `class BOT_ORM::Constraint`

- `BOT_ORM::Constraint::CompositeField`
- `BOT_ORM::Constraint::Default`
- `BOT_ORM::Constraint::Check`
- `BOT_ORM::Constraint::Unique`
- `BOT_ORM::Constraint::Reference`

## `ORMAP (TableName, PrimaryKey, ...)`

Before we use ORM Lite, we should **Inject** some code into the Class;

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

In this Sample, `ORMAP ("TableName", field1, ...)` specifies that:
- Class `MyClass` will be mapped into Table `TableName`;
- `field1, field2, field3, field4, field5, field6` will be mapped
  into `INTEGER field1 NOT NULL`, `REAL field2 NOT NULL`,
  `TEXT field3 NOT NULL`, `INTEGER field4`, `REAL field5`
  and `TEXT field6` respectively;
- The first entry `field1` will be set as the **Primary Key**
  of the Table;

Note that:
- You should Pass **at least 2** Params into this Macro
  (TableName and PrimaryKey), otherwise it will Not Compile...;
- Currently Only Support
  - T such that `std::is_integral<T>::value == true`
    and **NOT** `char` or `*char_t`
  - T such that `std::is_floating_point<T>::value == true`
  - T such that `std::is_same<T, std::string>::value == true`
  - which are mapped as `INTEGER`, `REAL` and `TEXT` (SQLite3);
- Not `Nullable` members will be mapped as `NOT NULL`;
- The **Primary Key** (first entry) is **Recommended** to be
  `Integral`, and it could be regarded as the `ROWID`
  with a Better Query **Performance** :-)
  (otherwise SQLite 3 will Generate a Column for `ROWID` Implicitly)
- Field Names MUST **NOT** be SQL Keywords (SQL Constraint);
- `std::string` Value MUST **NOT** contain `\0` (SQL Constraint);
- `ORMAP (...)` will **auto** Inject some **private members**;
  - `__Accept ()` to Implement **Visitor Pattern**;
  - `__Tuple ()` to **Flatten** data to tuple;
  - `__FieldNames ()` and `__TableName` to store strings;
  - and the Access by Implementation;

## `BOT_ORM::Nullable`

It keeps the *Similar Semantic* of `Nullable<T>` as `C#`; and
[Reference Here](https://stackoverflow.com/questions/2537942/nullable-values-in-c/28811646#28811646)

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

Return the `Underlying Value` of the object or
`Default Not-null Value` of `T`;
(similar to `GetValueOrDefault` in C#)

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

### Connection

``` cpp
ORMapper (const string &connectionString);
```

Remarks:
- Construct a **O/R Mapper** to connect to `connectionString`;
- For SQLite, the `connectionString` is the **database name**;
- The `ORMapper` **Keeps** the **Connection**
  and **Shares** the **Connection** with `Queryable`;
- **Disconnecting** at all related `ORMapper`/`Queryable` destructied;

### Transaction

``` cpp
void Transaction (Fn fn);
```

Remarks:
- Invoke `fn` **Transactionally**, as following:

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

### Create and Drop Table

``` cpp
// Create Table
void CreateTbl (const MyClass &);
void CreateTbl (const MyClass &,
                const Constraint &constraint1,
                const Constraint &constraint2,
                ...);

// Drop Table
void DropTbl (const MyClass &);
```

Remarks:
- Create/Drop Table for class `MyClass`;
- `void CreateTbl (const MyClass &, ...);`
  will Create a Table with **Constraints**;
- `Constraint` will be described later;

SQL:

``` sql
CREATE TABLE MyClass (...);

DROP TABLE MyClass;
```

### Insert

``` cpp
// Insert a single value
void Insert (const MyClass &entity, bool withId = true);

// Insert values
void InsertRange (const Container<MyClass> &entities, bool withId = true);
```

Remarks:
- Insert `entity` / `entities` into Table for `MyClass`;
- If `withId` is `false`, it will insert the `entity`
  without **Primary Key**;
  - Note that: **Primary Key** is recommended to be **Integral**
    in this case (**INT PK** would be `AUTOINCREMENT`,
    Floating Point / String may **Failed**);
- **NULL** Fields will **NOT** be Set;
- `entities` must **SUPPORT** `forward_iterator`;

SQL:

``` sql
INSERT INTO MyClass (...) VALUES (...);

INSERT INTO MyClass (...) VALUES (...);
INSERT INTO MyClass (...) VALUES (...);
...
```

### Update

``` cpp
// Update value by Primary Key
void Update (const MyClass &entity);

// Update values by Primary Key
void UpdateRange (const Container<MyClass> &entities);

// Update by Expressions
void Update (const MyClass &,
             const Expression::SetExpr &setExpr,
             const Expression::Expr &whereExpr);
```

Remarks:
- Update `entity` / `entities` in Table `MyClass`
  with the Same **Primary Key**;
- Update Set `setExpr` Where `whereExpr` for Table `MyClass`
  (`Expressions` will be described later);
- **NULL** Fields will also be Set;
- `entities` must **SUPPORT** `forward_iterator`;

SQL:

``` sql
UPDATE MyClass SET (...) WHERE KEY = <entity.id>;

UPDATE MyClass SET (...) WHERE KEY = <entity.id>;
UPDATE MyClass SET (...) WHERE KEY = <entity.id>;
...

UPDATE MyClass SET (...) WHERE ...;
```

### Delete

``` cpp
// Delete value by Primary Key
void Delete (const MyClass &entity);

// Delete by Expressions
void Delete (const MyClass &,
             const Expression::Expr &whereExpr);
```

Remarks:
- Delete Entry in Table `MyClass` with the Same **Primary Key**;
- Delete Where `whereExpr` for Table `MyClass`
  (`Expression` will be described later);
- This function will **NOT** throw a `std::runtime_error`
  even if there is nothing to delete;

SQL:

``` sql
DELETE FROM MyClass WHERE KEY = <entity.id>;

DELETE FROM MyClass WHERE ...;
```

### Query

``` cpp
// Retrieve a Queryable Object
Queryable<MyClass> Query (MyClass queryHelper);
```

Remarks:
- Return new `Queryable` object with `QueryResult` is `MyClass`;
- `MyClass` **MUST** be **Copy Constructible**
  to Construct a `queryHelper`;
- The `ORMapper` **Shares** the **Connection** with `Queryable`;

## `BOT_ORM::Queryable<QueryResult>`

### Retrieve Results

``` cpp
Nullable<T> Select (const Expression::Aggregate<T> &agg) const;
std::vector<QueryResult> ToVector () const;
std::list<QueryResult> ToList () const;
```

Remarks:
- `QueryResult` specifies the **Row Type** of Query Result;
- `Select` will Get the one-or-zero-row Result for `agg` immediately;
- `ToVector` / `ToList` returns the Collection of `QueryResult`;
- The results are from the **Connection** of `Queryable`;
- If the Result is `null` for `NOT Nullable` Field,
  it will throw `std::runtime_error`;
- `Expression` will be described later;

### Set Conditions

``` cpp
Queryable Distinct (bool isDistinct = true) const;
Queryable Where (const Expression::Expr &expr) const;

Queryable GroupBy (const Expression::Field<T> &field) const;
Queryable Having (const Expression::Expr &expr) const;

Queryable OrderBy (const Expression::Field<T> &field) const;
Queryable OrderByDescending (const Expression::Field<T> &field) const;

Queryable Take (size_t count) const;
Queryable Skip (size_t count) const;
```

Remarks:
- Default Selection is `ALL`;
- These functions will Set/Append Conditions to a copy of `this`;
- `OrderBy` will **Append** `field` to Condition,
  while Other functions will **Set** `DISTINCT`,
  `expr`, `field` or `count` to Condition;
- New `Queryable` **Shares** the **Connection** of `this`;
- `Expression` will be described later;

### Construct New `QueryResult`

``` cpp
auto Select (const Expression::Selectable<T1> &target1,
             const Expression::Selectable<T2> &target2,
             ...) const;
auto Join (const MyClass2 &queryHelper2,
           const Expression::Expr &onExpr) const;
auto LeftJoin (const MyClass2 &queryHelper2,
               const Expression::Expr &onExpr) const;
```

Remarks:
- Default Selection is **All Columns** (`SELECT *`);
- `Select` will Set `QueryResult` to `std::tuple<T1, T2, ...>`,
  which can be retrieved by `SELECT target1, target2, ...`
  (`target` can be **Field** or **Aggregate Functions**);
- `Join` / `LeftJoin` will Set `QueryResult` to `std::tuple<...>`
  - `...` is the **flattened nullable concatenation** of all entries of
    **Previous** `QueryResult` and `queryHelper2`;
  - **Flatten** and **Nullable** means all fields of `...` are
    all `Nullable<T>`, where `T` is the Supported Types of `ORMAP`
    (NOT `std::tuple` or `MyClass`);
  - `onExpr` specifies the `ON` Expression for `JOIN`;
- All Functions will copy the **Conditions** of `this` to the new one;
- New `Queryable` **Shares** the **Connection** of `this`;
- `Expression` will be described later;

### Compound Select

``` cpp
Queryable Union (const Queryable &queryable) const;
Queryable UnionAll (const Queryable &queryable) const;
Queryable Intersect (const Queryable &queryable) const;
Queryable Expect (const Queryable &queryable) const;
```

Remarks:
- All Functions will return a Compound Select
  for `this` and `queryable`;
- All Functions will only **Inherit Conditions**
  `OrderBy` and `Limit` from `this`;
- New `Queryable` **Shares** the **Connection** of `this`;

### Query SQL

We will use the following SQL to Query:

``` sql
SELECT [DISTINCT] ...
FROM TABLE
     [LEFT] JOIN TABLE ON ...
     ...
WHERE ...
GROUP BY <field>
HAVING ...
[<Compound> SELECT ... FROM ...]
ORDER BY <field> [DESC], ...
LIMIT <take> OFFSET <skip>;
```

Remarks:
- If the Corresponding Condition is NOT Set, it will be omitted;

## `BOT_ORM::FieldExtractor`

``` cpp
// Construction
FieldExtractor (const MyClass1 &queryHelper1,
                const MyClass2 &queryHelper2,
                ...);

// Get Field<> by operator ()
Field<T> operator () (const T &field) const;
NullableField<T> operator () (const Nullable<T> &field) const;
```

Remarks:
- Construction of `FieldExtractor` will take all fields' pointers of
  `queryHelper` into a **Hash Table**;
- `operator () (field)` will find the position of `field`
  in the **Hash Table** from `queryHelper`
  and Construct the corresponding `Field`;
- If the `field` is `Nullable<T>`
  it will Construct a `NullableField<T>`;
  and it will Construct a `Field<T>` otherwise;
- If `field` is not a member of `queryHelper`,
  it will throw `std::runtime_error`;
- `Expression` will be described later;

## `namespace BOT_ORM::Expression`

### Fields and Aggregate Functions

#### Definitions

``` cpp
BOT_ORM::Expression::Selectable<T>
BOT_ORM::Expression::Field<T> : public Selectable<T>
BOT_ORM::Expression::NullableField<T> : public Field<T>
BOT_ORM::Expression::Aggregate<T> : public Selectable<T>
```

#### Operations

``` cpp
// Field / Aggregate ? Value
Expr operator == (const Selectable<T> &op, T value);
Expr operator != (const Selectable<T> &op, T value);
Expr operator >  (const Selectable<T> &op, T value);
Expr operator >= (const Selectable<T> &op, T value);
Expr operator <  (const Selectable<T> &op, T value);
Expr operator <= (const Selectable<T> &op, T value);

// Field ? Field
Expr operator == (const Field<T> &op1, const Field<T> &op2);
Expr operator != (const Field<T> &op1, const Field<T> &op2);
Expr operator >  (const Field<T> &op1, const Field<T> &op2);
Expr operator >= (const Field<T> &op1, const Field<T> &op2);
Expr operator <  (const Field<T> &op1, const Field<T> &op2);
Expr operator <= (const Field<T> &op1, const Field<T> &op2);

// Nullable Field ? nullptr
Expr operator == (const NullableField<T> &op, nullptr_t);
Expr operator !== (const NullableField<T> &op, nullptr_t);

// String Field ? std::string
Expr operator& (const Field<std::string> &op, std::string val);
Expr operator| (const Field<std::string> &op, std::string val);

// Get SetExpr
SetExpr operator = (const Field<T> &op, T value);
SetExpr operator = (const NullableField<T> &op, nullptr_t);
```

Remarks:
- `Selectable<T> ? T` returns `Expr<op ? value>`;
- `Field<T> ? Field<T>` returns `Expr<op1 ? op2>`;
- `NullableField<T> == / != nullptr`
  returns `Expr<op> IS NULL / IS NOT NULL`;
- `Field<std::string> & / | T`
  returns `Expr<op> LIKE / NOT LIKE <value>`;
- `Field<T> = T` returns `SetExpr<op> = <value>`;
- `NullableField<T> = nullptr` returns `SetExpr<op> = null`;

### Expressions

#### Definitions

``` cpp
BOT_ORM::Expression::Expr
BOT_ORM::Expression::SetExpr
```

#### Operations

``` cpp
// Get Composite Expr
Expr operator && (const Expr &op1, const Expr &op2);
Expr operator || (const Expr &op1, const Expr &op2);

// Concatenate 2 SetExpr
SetExpr operator && (const SetExpr &op1, const SetExpr &op2);
```

Remarks:
- `Expr && / || Expr` returns `(<op1> and / or <op2>)`;
- `SetExpr && SetExpr` returns `<op1>, <op2>`;

### Aggregate Function Helpers

``` cpp
BOT_ORM::Expression::Count ();
BOT_ORM::Expression::Count (const Field<T> &field);
BOT_ORM::Expression::Sum (const Field<T> &field);
BOT_ORM::Expression::Avg (const Field<T> &field);
BOT_ORM::Expression::Max (const Field<T> &field);
BOT_ORM::Expression::Min (const Field<T> &field);
```

Remarks:

They will Generate Aggregate Functions as:

- `size_t COUNT (*)`
- `size_t COUNT (field)`
- `T SUM (field)`
- `T AVG (field)`
- `T MAX (field)`
- `T MIN (field)`

## `class BOT_ORM::Constraint`

### Composite Field

`CompositeField` could be constructed from normal `Field`
and is used by **Constraint Functions**;

``` cpp
CompositeField (const Expression::Field<T1> &field1,
                const Expression::Field<T2> &field2,
                ...);
```

Remarks:
- Composite Field will Contain fields in the given **Order**;
- If the `field`s are not from the Same Table,
  it will throw `std::runtime_error`;

### Generate Constraints

``` cpp
Constraint Default (const Expression::Field<T> &field,
                    const T &value);
Constraint Check (const Expression::Expr &expr);
Constraint Unique (const Expression::Field<T> &field);
Constraint Unique (const CompositeField &fields);
Constraint Reference(const Expression::Field<T> &field,
                     const Expression::Field<T> &refered);
Constraint Reference (const CompositeField &field,
                      const CompositeField &refered);
```

Remarks:
- They will Generate Constraints as:
  `DEFAULT`, `CHECK`, `UNIQUE` and `FOREIGN KEY`;
- `FOREIGN KEY` is Enabled by Default
  (during the Construction of `ORMapper`);
- Why there is NO **Composite Primary Key** Constaints:
  - It's recommended to use a **Integral** Field as the Primary Key,
    described in Section ## Macro `ORMAP`;
  - We can use `Unique` to implement a **Composite Unique** Constraint;
  - And use `Reference` to define a **Composite Foreign Key**;

## Error Handling

### Compile-time Error

ORM Lite uses `static_assert` to Check if the Code is valid:

- **Forget** to **Place** `ORMAP` into the Class
  > Please Inject the Class with 'ORMAP' first
- Place **Unsupported Types** into `ORMAP`
  > Only Support Integral, Floating Point and std::string

Note that: Error Messages will often appear at the **TOP**;

### Runtime Error

All Functions will throw `std::runtime_error`
with the **Error Message** if Failed:

- Failed to **Connect** to **Database**
  > SQL error: Can't open database `<connectionString>`
- Failed at Executing **Query** Script
  > SQL error: `<ErrorMessage>` at `<Generated SQL Script>`
- Pass a **Non-Member** Var of Registered Object to Field **Extractor**
  > No Such Field for current Extractor
- Get `NULL` from Query while the Expected **Field** is **NOT NULL**
  (happening in **NOT** *Code First* Cases...)
  > Get Null Value for NOT Nullable Type
- **Composite** Fields from **NOT** the Same Tables
  > Fields are NOT from the Same Table