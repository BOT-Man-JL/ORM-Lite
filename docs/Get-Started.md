# Get Started

Here is a **short tour** for this **Amazing** ORM 😉

The full code is in [Sample.cpp](../Sample.cpp).

## Preparation

Before we start, Include **src** into your Project:

- `ORMLite.h`
- **SQLite3** Dependency

> If you haven't installed **SQLite3**,
> you can find the zip file of `sqlite3.h` and `sqlite3.c` in `/src`

## Including *ORM Lite*

``` cpp
#include "ORMLite.h"
using namespace BOT_ORM;
using namespace BOT_ORM::Expression;

struct UserModel
{
    int user_id;
    std::string user_name;
    double credit_count;

    Nullable<int> age;
    Nullable<double> salary;
    Nullable<std::string> title;

    // Inject ORM-Lite into this Class :-)
    ORMAP ("UserModel", user_id, user_name, credit_count,
           age, salary, title);
};
```

`Nullable<T>` helps us construct `Nullable` Value in C++,
which is described in the [Document](ORM-Lite.md) 😁

In this Sample, `ORMAP ("UserModel", ...)` do that:
- `Class UserModel` will be mapped into `TABLE UserModel`;
- NOT `Nullable` members will be mapped as `NOT NULL`;
- `int, double, std::string` will be mapped into
  `INT, REAL, TEXT` respectively;
- The first entry `id` will be set as the **Primary Key** of the Table;

And then we define and inject other 2 Classes in this Sample:

``` cpp
struct SellerModel
{
    int seller_id;
    std::string seller_name;
    double credit_count;

    // Inject ORM-Lite into this Class :-)
    ORMAP ("SellerModel", seller_id, seller_name, credit_count);
};

struct OrderModel
{
    int order_id;
    int user_id;
    int seller_id;
    std::string product_name;
    Nullable<double> fee;

    // Inject ORM-Lite into this Class :-)
    ORMAP ("OrderModel", order_id, user_id, seller_id,
           product_name, fee);
};
```

## Field Extracting

- `FieldExtractor`

``` cpp
// Define more Query Helper Objects and their Field Extractor
UserModel user;
SellerModel seller;
OrderModel order;
auto field = FieldExtractor { user, seller, order };

// Extract Field from 'field'
// For example: field (user.user_name)
//   => Retrieve the field of user_name in UserModel table
```

## Working on *Database* with *ORMapper*

- `ORMapper`

``` cpp
// Open a Connection with 'Sample.db'
ORMapper mapper ("Sample.db");
```

## Create or Drop Tables

- `ORMapper.CreateTbl`
- `ORMapper.DropTbl`
- `Expression`

``` cpp
// Create Table with Constraints
mapper.CreateTbl (
    UserModel {},
    Constraint::Default (field (user.salary), 1000.0));

// Remarks:
// CREATE TABLE UserModel(
//   user_id INTEGER NOT NULL PRIMARY KEY,
//   user_name TEXT NOT NULL,
//   credit_count REAL NOT NULL,
//   age INTEGER,
//   salary REAL DEFAULT 1000,
//   title TEXT);

mapper.CreateTbl (
    SellerModel {},
    Constraint::Check (field (seller.credit_count) > 0.0));

// Remarks:
// CREATE TABLE SellerModel(
//   seller_id INTEGER NOT NULL PRIMARY KEY,
//   seller_name TEXT NOT NULL,
//   credit_count REAL NOT NULL,
//   CHECK (credit_count > 0));

mapper.CreateTbl (
    OrderModel {},
    Constraint::Reference (
        field (order.user_id), field (user.user_id)),
    Constraint::Reference (
        field (order.seller_id), field (seller.seller_id)),
    Constraint::Unique (Constraint::CompositeField {
        field (order.product_name), field (order.fee) }));

// Remarks:
// CREATE TABLE OrderModel(
//   order_id INTEGER NOT NULL PRIMARY KEY,
//   user_id INTEGER NOT NULL,
//   seller_id INTEGER NOT NULL,
//   product_name TEXT NOT NULL,
//   fee REAL,
//   FOREIGN KEY (user_id) REFERENCES UserModel(user_id),
//   FOREIGN KEY (seller_id) REFERENCES SellerModel(seller_id),
//   UNIQUE (product_name, fee));

...

// Drop Tables
mapper.DropTbl (UserModel {});
mapper.DropTbl (SellerModel {});
mapper.DropTbl (OrderModel {});
```

#### User Model

| user_id| user_name| credit_count|    age|  salary|  title|
|--------|----------|-------------|-------|--------|-------|
|       0|      John|          0.2|     21|  1000.0| `null`|
|       1|      Jack|          0.4| `null`|    3.14| `null`|
|       2|      Jess|          0.6| `null`|  1000.0|    Dr.|
|     ...|       ...|          ...|    ...|     ...|    ...|

#### Seller Model

| seller_id| seller_name| credit_count|
|----------|------------|-------------|
|       ...|         ...|          ...|

#### Order Model

| order_id| user_id| seller_id| product_name| fee|
|---------|--------|----------|-------------|----|
|      ...|     ...|       ...|          ...| ...|

## Basic CRUD

- `ORMapper.Insert` (Create)
- `ORMapper.Query`  (Read)
- `ORMapper.Update` (Update)
- `ORMapper.Delete` (Delete)
- `ORMapper.Transaction`
- `Table Constraints`

``` cpp
std::vector<UserModel> initObjs =
{
    { 0, "John", 0.2, 21, nullptr, nullptr },
    { 1, "Jack", 0.4, nullptr, 3.14, nullptr },
    { 2, "Jess", 0.6, nullptr, nullptr, std::string ("Dr.") }
};

// Insert Values with Primary Key
for (const auto &obj : initObjs)
    mapper.Insert (obj);

initObjs[1].salary = nullptr;
initObjs[1].title = "St.";

// Update Entity by Primary Key (WHERE UserModel.id = 1)
mapper.Update (initObjs[1]);

// Delete Entity by Primary Key (WHERE UserModel.id = 2)
mapper.Delete (initObjs[2]);

// Transactional Statements
try
{
    mapper.Transaction ([&] ()
    {
        mapper.Delete (initObjs[0]);  // OK
        mapper.Insert (UserModel {
            1, "Joke", 0, nullptr, nullptr, nullptr
        });  // Failed
    });
}
catch (const std::exception &ex)
{
    // If any statement Failed, throw an exception

    std::cout << ex.what () << std::endl;
    // SQL error: 'UNIQUE constraint failed: UserModel.id'

    // Remarks:
    // mapper.Delete (initObjs[0]); will not applied :-)
}

// Select All to List
auto result1 = mapper.Query (UserModel {}).ToList ();

// decltype (result1) == std::list<UserModel>
// result1 = [{ 0, 0.2, "John", 21,   1000, null  },
//            { 1, 0.4, "Jack", null, null, "St." }]

// Table Constraints
try
{
    // Insert Values without Primary Key
    mapper.Insert (SellerModel { 0, "John Inc.", 0.0 }, false);
}
catch (const std::exception &ex)
{
    std::cout << ex.what () << std::endl;
    // SQL error: 'CHECK constraint failed: SellerModel'
}
```

## Batch Operations

- `ORMapper.InsertRange`
- `ORMapper.UpdateRange`

``` cpp
std::vector<UserModel> dataToSeed;
for (int i = 50; i < 100; i++)
    dataToSeed.emplace_back (UserModel {
        i, "July_" + std::to_string (i), i * 0.2,
        nullptr, nullptr, nullptr
});

// Insert by Batch Insert
mapper.Transaction ([&] () {
    mapper.InsertRange (dataToSeed);
});

for (size_t i = 0; i < 20; i++)
{
    dataToSeed[i + 30].age = 30 + (int) i / 2;
    dataToSeed[i + 20].title = "Mr. " + std::to_string (i);
}

// Update by Batch Update
mapper.Transaction ([&] () {
    // Note that: it will erase the default value of 'salary'
    mapper.UpdateRange (dataToSeed);
});
```

## Single-Table Query

- `Expression`
- `Aggregate`
- `Queryable.Where`
- `Queryable.OrderBy`
- `Queryable.Take`
- `Queryable.Skip`
- `Queryable.Select`

``` cpp
// Select by Condition
auto result2 = mapper.Query (UserModel {})
    .Where (
        field (user.user_name) & std::string ("July%") &&
        (field (user.age) >= 32 &&
         field (user.title) != nullptr)
    )
    .OrderByDescending (field (user.age))
    .OrderBy (field (user.user_id))
    .Take (3)
    .Skip (1)
    .ToVector ();

// Remarks:
// SELECT * FROM UserModel
// WHERE (user_name LIKE 'July%' AND
//       (age >= 32 AND title IS NOT NULL))
// ORDER BY age DESC, user_id
// LIMIT 3 OFFSET 1

// decltype (result2) == std::vector<UserModel>
// result2 = [{ 89, 17.8, "July_89", 34, null, "Mr. 19" },
//            { 86, 17.2, "July_86", 33, null, "Mr. 16" },
//            { 87, 17.4, "July_87", 33, null, "Mr. 17" }]

// Calculate Aggregate Function
auto avg = mapper.Query (UserModel {})
    .Where (field (user.user_name) & std::string ("July%"))
    .Aggregate (Avg (field (user.credit_count)));

// Remarks:
// SELECT AVG (credit_count) FROM UserModel
// WHERE (user_name LIKE 'July%')

// avg = 14.9

auto count = mapper.Query (UserModel {})
    .Where (field (user.user_name) | std::string ("July%"))
    .Aggregate (Count ());

// Remarks:
// SELECT COUNT (*) FROM UserModel
// WHERE (user_name NOT LIKE 'July%')

// count = 2
```

## Update / Delete by Statement

- `Expression`
- `ORMapper.Update`
- `ORMapper.Delete`

``` cpp
// Update by Condition
mapper.Update (
    UserModel {},
    (field (user.age) = 10) &&
    (field (user.credit_count) = 1.0),
    field (user.user_name) == std::string ("July"));

// Remarks:
// UPDATE UserModel SET age = 10,credit_count = 1.0
// WHERE (user_name = 'July')

// Delete by Condition
mapper.Delete (UserModel {},
               field (user.user_id) >= 90);

// Remarks:
// DELETE FROM UserModel WHERE (id >= 90)
```

## Multi-Table Query

- `Expression`
- `Aggregate`
- `Queryable.Join`
- `Queryable.LeftJoin`
- `Queryable.Select`
- `Queryable.GroupBy`
- `Queryable.Having`
- `Queryable.Union`

``` cpp
mapper.Transaction ([&] ()
{
    for (size_t i = 0; i < 50; i++)
    {
        mapper.Insert (
            SellerModel { (int) i + 50,
            "Seller " + std::to_string (i), 3.14 });
        mapper.Insert (
            OrderModel { 0,
            (int) i / 2 + 50,
            (int) i / 4 + 50,
            "Item " + std::to_string (i),
            i * 0.5 }, false);
    }
});

// Join Tables for Query
auto joinedQuery = mapper.Query (UserModel {})
    .Join (OrderModel {},
           field (user.user_id) ==
           field (order.user_id))
    .LeftJoin (SellerModel {},
               field (seller.seller_id) ==
               field (order.seller_id))
    .Where (field (user.user_id) >= 65);

// Get Result to List
auto result3 = joinedQuery.ToList ();

// Remarks:
// SELECT * FROM UserModel
//               JOIN OrderModel
//               ON UserModel.user_id=OrderModel.user_id
//               LEFT JOIN SellerModel
//               ON SellerModel.seller_id=OrderModel.seller_id
// WHERE (UserModel.user_id>=65)

// decltype (result3) == std::list<std::tuple<Nullable<..>, ..>>
// result3 = [(65, "July_65", 13, null, null, null,
//             31, 65, 57, "Item 30", 15,
//             57, "Seller 7", 3.14),
//            (65, "July_65", 13, null, null, null,
//             32, 65, 57, "Item 31", 15.5,
//             57, "Seller 7", 3.14),
//            ... ]

// Group & Having ~
auto result4 = joinedQuery
    .Select (field (order.user_id),
             field (user.user_name),
             Avg (field (order.fee)))
    .GroupBy (field (user.user_name))
    .Having (Sum (field (order.fee)) >= 40.5)
    .Skip (3)
    .ToList ();

// Remarks:
// SELECT OrderModel.user_id,
//        UserModel.user_name,
//        AVG (OrderModel.fee)
// FROM UserModel
//      JOIN OrderModel
//      ON UserModel.user_id=OrderModel.user_id
//      LEFT JOIN SellerModel
//      ON SellerModel.seller_id=OrderModel.seller_id
// WHERE (UserModel.user_id>=65)
// GROUP BY UserModel.user_name
// HAVING SUM (OrderModel.fee)>=40.5
// LIMIT ~0 OFFSET 3

// decltype (result4) == std::list<std::tuple<Nullable<..>, ..>>
// result4 = [(73, "July_73", 23.25),
//            (74, "July_74", 24.25)]

// Compound Select
// Results are Nullable-Tuples
auto result5 = mapper.Query (OrderModel {})
    .Select (field (order.product_name), field (order.user_id))
    .Where (field (order.user_id) == 50)
    .Union (
        joinedQuery
        .Select (field (user.user_name), field (order.order_id))
    )
    .Take (4)
    .ToList ();

// Remarks:
// SELECT OrderModel.product_name,
//        OrderModel.user_id
// FROM OrderModel
//      WHERE (OrderModel.user_id==50)
// UNION
// SELECT UserModel.user_name,
//        OrderModel.order_id
// FROM UserModel
//      JOIN OrderModel
//      ON UserModel.user_id=OrderModel.user_id
//      LEFT JOIN SellerModel
//      ON SellerModel.seller_id=OrderModel.seller_id
//      WHERE (UserModel.user_id>=65)
// LIMIT 4;

// decltype (result5) == std::list<std::tuple<Nullable<..>, ..>>
// result5 = [("Item 0", 50),
//            ("Item 1", 50),
//            ("July_65", 31),
//            ("July_65", 32)]
```

## Wrapping Up

This little ORM is very cool, enjoy it. 😇

See the **[Full Document Here](ORM-Lite.md)**.