
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

	// Store the Data in "test.db"
	ORMapper<MyClass> mapper ("test.db");

	// Create a table for "MyClass"
	mapper.CreateTbl ();

	// Insert Values into the table
	std::vector<MyClass> initObjs =
	{
		{ 0, 0.2, "John" },
		{ 1, 0.4, "Jack" },
		{ 2, 0.6, "Jess" }
	};
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
			Expr (_mc.name, "=", "July") &&
			(
				Expr (_mc.id, "<=", 90) &&
				Expr (_mc.id, ">=", 60)
			)
		)
		.OrderBy (_mc.id, true)
		.Limit (3, 10)
		.ToVector ();

	// Select by SQL, NOT Recommended :-(
	std::vector<MyClass> query2;
	mapper.Select (query2,
				   "where (name='July' and (id<=90 and id>=60))"
				   " order by id desc"
				   " limit 3 offset 10");

	// Note that: query1 = query2 =
	// [{ 80, 17.0, "July"}, { 79, 16.8, "July"}, { 78, 16.6, "July"}]

	// Count by Query :-)
	auto count1 = mapper.Query (_mc)    // Link '_mc' to its fields
		// Auto Cosntruct Expr { _mc.name, "=", "July" } :-)
		.Where ({ _mc.name, "=", "July" })
		.Count ();

	// Count by SQL, NOT Recommended :-(
	auto count2 = mapper.Count ("where (name='July')");

	// Note that:
	// count1 = count2 = 50

	// Delete by Query :-)
	mapper.Query (_mc)                  // Link '_mc' to its fields
		// Auto Cosntruct Expr { _mc.name, "=", "July" } :-)
		.Where ({ _mc.name = "July" })
		.Delete ();

	// Delete by SQL, NOT Recommended :-(
	mapper.Delete ("where (name='July')");

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
	printVec (query2);
	std::cout << count1 << " " << count2 << std::endl;

	std::cin.get ();
	return 0;
}
