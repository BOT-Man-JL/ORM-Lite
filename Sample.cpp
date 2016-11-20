
// A Sample of ORM Lite
// https://github.com/BOT-Man-JL/ORM-Lite
// BOT Man, 2016

#include <string>
#include <iostream>

#include "src/ORMLite.h"
using namespace BOT_ORM;

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

int main ()
{
	// Open a Connection with *test.db*
	ORMapper mapper ("test.db");

	// Create a table for "MyClass"
	mapper.CreateTbl (MyClass {});

	/* #1 Basic Usage */

	std::vector<MyClass> initObjs =
	{
		{ 0, 0.2, "John", 21, nullptr, nullptr },
		{ 1, 0.4, "Jack", nullptr, 3.14, nullptr },
		{ 2, 0.6, "Jess", nullptr, nullptr, std::string ("Dr.") }
	};

	// Insert Values into the table
	for (const auto obj : initObjs)
		mapper.Insert (obj);

	// Update Entry by KEY (id)
	initObjs[1].salary = nullptr;
	initObjs[1].title = "St.";
	mapper.Update (initObjs[1]);

	// Delete Entry by KEY (id)
	mapper.Delete (initObjs[2]);

	// Transactional Statements
	try
	{
		mapper.Transaction ([&] ()
		{
			mapper.Delete (initObjs[0]);
			mapper.Insert (MyClass { 1, 0, "Joke" });
		});
	}
	catch (const std::exception &ex)
	{
		// If any statement Failed, throw an exception
		// "SQL error: UNIQUE constraint failed: MyClass.id"

		// Remarks:
		// mapper.Delete (initObjs[0]); will not applied :-)
	}

	// Select All to Vector
	auto result1 = mapper.Query (MyClass {}).ToVector ();
	// result1 = [{ 0, 0.2, "John", 21,   null, null  },
	//            { 1, 0.4, "Jack", null, null, "St." }]

	/* #2 Batch Operations */

	// Insert by Batch Insert
	// Performance is much Better than Separated Insert :-)
	std::vector<MyClass> dataToSeed;
	for (int i = 50; i < 100; i++)
		dataToSeed.emplace_back (MyClass { i, i * 0.2, "July" });
	mapper.Transaction ([&] ()
	{
		mapper.InsertRange (dataToSeed);
	});

	// Update by Batch Update
	for (size_t i = 0; i < 50; i++)
		dataToSeed[i].score += 1;
	mapper.Transaction ([&] ()
	{
		mapper.UpdateRange (dataToSeed);
	});

	/* #3 Composite Query */

	// Define a Query Helper Object
	MyClass helper;

	// Select by Query :-)
	auto result2 = mapper.Query (helper)   // Link 'helper' to its fields
		.Where (
			Field (helper.name) == "July" &&
			(Field (helper.id) <= 90 && Field (helper.id) >= 60)
		)
		.OrderByDescending (helper.id)
		.Take (3)
		.Skip (10)
		.ToVector ();

	// Remarks:
	// sql = SELECT * FROM MyClass
	//       WHERE (name='July' and (id<=90 and id>=60))
	//       ORDER BY id DESC
	//       LIMIT 3 OFFSET 10
	// result2 =
	// [{ 80, 17.0, "July", null, null, null },
	//  { 79, 16.8, "July", null, null, null },
	//  { 78, 16.6, "July", null, null, null }]

	// Reusable Query Object :-)
	auto query = mapper.Query (helper)     // Link 'helper' to its fields
		.Where (Field (helper.name) == "July");

	// Aggregate Function by Query :-)
	auto count = query.Count ();

	// Remarks:
	// sql = SELECT COUNT (*) FROM MyClass WHERE (name='July')
	// count = 50

	// Update by Query :-)
	query.Update (helper.score = 10, helper.name = "Jully");

	// Remarks:
	// sql = UPDATE MyClass SET score=10,name='Jully' WHERE (name='July')

	// Delete by Query :-)
	mapper.Query (helper)                  // Link 'helper' to its fields
		.Where (helper.name = "Jully")     // Trick ;-) (Detailed in Docs)
		.Delete ();

	// Remarks:
	// sql = DELETE FROM MyClass WHERE (name='Jully')

	// ==========

	// Drop the table "MyClass"
	mapper.DropTbl (MyClass {});

	// Output to Console
	auto printVec = [] (const std::vector<MyClass> vec)
	{
		auto printNullable = [] (std::ostream &os, const auto &val)
			-> std::ostream &
		{
			if (val == nullptr)
				return os << "null";
			else
				return os << val.Value ();
		};

		for (auto& item : vec)
		{
			std::cout << item.id << "\t" << item.score
				<< "\t" << item.name << "\t";
			printNullable (std::cout, item.age) << "\t";
			printNullable (std::cout, item.salary) << "\t";
			printNullable (std::cout, item.title) << std::endl;
		}
		std::cout << std::endl;
	};

	printVec (result1);
	printVec (result2);
	std::cout << count << std::endl;

	std::cin.get ();
	return 0;
}
