
#include <string>
#include <iostream>

#include "src/ORMLite.h"
using namespace BOT_ORM;

class MyClass
{
	// Inject ORM-Lite into this Class
	ORMAP (MyClass, id, real, str)
public:
	long id;
	double real;
	std::string str;
};

int main ()
{
	// Store the Data in "test.db"
	ORMapper<MyClass> mapper ("test.db");

	// Create a table for "MyClass"
	mapper.CreateTbl ();

	// Insert Values into the table
	mapper.Insert (MyClass { 1, 0.2, "John" });
	mapper.Insert (MyClass { 2, 0.4, "Jack" });
	mapper.Insert (MyClass { 3, 0.6, "Jess" });
	mapper.Insert (MyClass { 4, 0.8, "July" });
	mapper.Insert (MyClass { 5, 1.0, "July" });

	// Update Entry by KEY (id)
	mapper.Update (MyClass { 2, 0.6, "Jack" });

	// Delete Entry by KEY (id)
	mapper.Delete (MyClass { 3, 0.6, "Jess" });

	// Query Entries
	std::vector<MyClass> query1;
	mapper.Query (query1);
	// query1 = [MyClass { 1, 0.2, "John" },
	//           MyClass { 2, 0.6, "Jack" },
	//           MyClass { 4, 0.8, "July" },
	//           MyClass { 5, 1.0, "July" }]

	// Count Entries
	auto count1 = mapper.Count ();  // count = 4

	// Query Entries by Condition
	std::vector<MyClass> query2;
	mapper.Query (query2, "where str='July' order by real desc");
	// query2 = [MyClass { 5, 1.0, "July" },
	//           MyClass { 4, 0.8, "July" }]

	// Delete Entries by Condition
	mapper.Delete ("where str='July'");

	// Count Entries by Condition
	auto count2 = mapper.Count ("where str='July'");  // count = 0

	// View the latest Error Message
	mapper.Insert (MyClass { 1, 0, "Admin" });
	auto errStr = mapper.ErrMsg ();
	// errStr = "SQL error: UNIQUE constraint failed: MyClass.id"

	// Drop the table "MyClass"
	mapper.DropTbl ();

	getchar ();
	return 0;
}
