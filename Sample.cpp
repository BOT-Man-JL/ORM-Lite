
// A Sample of ORM Lite
// https://github.com/BOT-Man-JL/ORM-Lite
// BOT Man, 2016

#include <string>
#include <iostream>

#include "src/ORMLite.h"
using namespace BOT_ORM;

/* #0 Basic Usage */

struct MyClass
{
	long id;
	double score;
	std::string name;

	// Inject ORM-Lite into this Class
	ORMAP (MyClass, id, score, name)
};

int main ()
{
	/* #1 Basic Usage */
	std::vector<MyClass> initObjs =
	{
		{ 0, 0.2, "John" },
		{ 1, 0.4, "Jack" },
		{ 2, 0.6, "Jess" }
	};

	// Open a Connection with *test.db*
	ORMapper<MyClass> mapper ("test.db");

	// Create a table for "MyClass"
	mapper.CreateTbl ();

	// Insert Values into the table
	for (const auto obj : initObjs)
		mapper.Insert (obj);

	// Update Entry by KEY (id)
	initObjs[1].score = 1.0;
	mapper.Update (initObjs[1]);

	// Delete Entry by KEY (id)
	mapper.Delete (initObjs[2]);

	// Select All to Vector
	auto query0 = mapper.Query (MyClass ()).ToVector ();
	// query0 = [{ 0, 0.2, "John"},
	//           { 1, 1.0, "Jack"}]

	// If 'Insert' Failed, Print the latest Error Message
	if (!mapper.Insert (MyClass { 1, 0, "Joke" }))
		auto err = mapper.ErrMsg ();
	// err = "SQL error: UNIQUE constraint failed: MyClass.id"

	/* #2 Batch Operations */

	// Insert by Batch Insert
	// Performance is much Better than Separated Insert :-)
	std::vector<MyClass> dataToSeed;
	for (long i = 50; i < 100; i++)
		dataToSeed.emplace_back (MyClass { i, i * 0.2, "July" });
	mapper.Insert (dataToSeed);

	// Update by Batch Update
	for (size_t i = 0; i < 50; i++)
		dataToSeed[i].score += 1;
	mapper.Update (dataToSeed);

	/* #3 Composite Query */

	// Define a Query Helper Object
	MyClass _mc;

	// Select by Query :-)
	auto query1 = mapper.Query (_mc)    // Link '_mc' to its fields
		.Where (
			Field (_mc.name) == "July" &&
			(Field (_mc.id) <= 90 && Field (_mc.id) >= 60)
		)
		.OrderBy (_mc.id, true)
		.Limit (3, 10)
		.ToVector ();

	// Remarks:
	// sql = SELECT * FROM MyClass
	//       WHERE (name='July' and (id<=90 and id>=60))
	//       ORDER BY id DESC
	//       LIMIT 3 OFFSET 10
	// query1 =
	// [{ 80, 17.0, "July"}, { 79, 16.8, "July"}, { 78, 16.6, "July"}]

	// Count by Query :-)
	auto count = mapper.Query (_mc)    // Link '_mc' to its fields
		.Where (Field (_mc.name) == "July")
		.Count ();

	// Remarks:
	// sql = SELECT COUNT (*) AS NUM FROM MyClass WHERE (name='July')
	// count = 50

	// Delete by Query :-)
	mapper.Query (_mc)                  // Link '_mc' to its fields
		.Where (_mc.name = "July")      // Trick ;-)
		.Delete ();

	// Remarks:
	// sql = DELETE FROM MyClass WHERE (name='July')

	// Drop the table "MyClass"
	mapper.DropTbl ();

	// Output to Console
	auto printVec = [] (const std::vector<MyClass> vec)
	{
		for (auto& item : vec)
			std::cout << item.id << "\t" << item.score
			<< "\t" << item.name << std::endl;
		std::cout << std::endl;
	};
	printVec (query0);
	printVec (query1);
	std::cout << count << std::endl;

	std::cin.get ();
	return 0;
}
