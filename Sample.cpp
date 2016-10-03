
#include <string>
#include <iostream>

#include "src/ORMLite.h"

class MyClass
{
	ORMAP (MyClass, id, real, str)

public:
	long id;
	double real;
	std::string str;
};

class MyClass2
{
	ORMAP (MyClass2, id, age, score, firstName, lastName)

public:
	long id, age;
	double score;
	std::string firstName, lastName;
	int dummyInt;
};

int main ()
{
	using namespace BOT_ORM;
	// Store the Data in "test.db"
	ORMapper<MyClass> mapper ("test.db");

	// Create a table for "MyClass"
	mapper.CreateTbl ();

	// Insert Values into the table
	mapper.Insert (MyClass { 1, 0.2, "John" });
	mapper.Insert (MyClass { 2, 0.4, "Jack" });
	mapper.Insert (MyClass { 3, 0.6, "Jess" });

	// Update the Value by its KEY (id)
	mapper.Update (MyClass { 2, 0.6, "Jack" });

	// Delete the Value by its KEY (id)
	mapper.Delete (MyClass { 3, 0.6, "Jess" });

	// Query the Entries in the table, and Set to entries
	std::vector<MyClass> entries;
	mapper.Query (entries);  // entries = [MyClass { 1, 0.2, "John" },
							 //            MyClass { 2, 0.6, "Jack" }]

							 // Get the Count of Entries in the table
	auto count = mapper.Count ();  // count = 2

								   // Drop the table "MyClass"
								   //mapper.DropTbl ();

								   // View the latest Error Message
	mapper.ErrMsg ();

	getchar ();
	return 0;
}
