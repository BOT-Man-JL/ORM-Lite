
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

	// Select All to List
	auto result1 = mapper.Query (MyClass {}).ToList ();
	//   result1 = [{ 0, 0.2, "John", 21,   null, null  },
	//              { 1, 0.4, "Jack", null, null, "St." }]

	/* #2 Batch Operations */

	std::vector<MyClass> dataToSeed;
	for (int i = 50; i < 100; i++)
		dataToSeed.emplace_back (MyClass { i, i * 0.2, "July" });

	// Insert by Batch Insert
	mapper.Transaction ([&] () {
		mapper.InsertRange (dataToSeed);
	});

	for (size_t i = 0; i < 20; i++)
	{
		dataToSeed[i + 30].age = 30 + (int) i;
		dataToSeed[i + 20].title = "Mr. " + std::to_string (i);
	}

	// Update by Batch Update
	mapper.Transaction ([&] () {
		mapper.UpdateRange (dataToSeed);
	});

	/* #3 Composite Query */

	// Define a Query Helper Object
	MyClass helper;

	// Select by Query :-)
	auto result2 = mapper.Query (helper)   // Link 'helper' to its fields
		.Where (
			Field (helper.name) == "July" &&
			(Field (helper.age) >= 35 && Field (helper.title) != nullptr)
		)
		.OrderByDescending (helper.id)
		.Take (3)
		.Skip (1)
		.ToVector ();

	// Remarks:
	// sql = SELECT * FROM MyClass
	//       WHERE ((name='July' AND (age>=35 AND title IS NOT NULL)))
	//       ORDER BY id DESC
	//       LIMIT 3 OFFSET 1
	// result2 = [{ 88, 17.6, "July", 38, null, "Mr. 18" },
	//            { 87, 17.4, "July", 37, null, "Mr. 17" },
	//            { 86, 17.2, "July", 36, null, "Mr. 16" }]

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
	auto printResult = [] (const auto &vec)
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

	printResult (result1);
	printResult (result2);
	std::cout << count << std::endl;

	std::cin.get ();
	return 0;
}
