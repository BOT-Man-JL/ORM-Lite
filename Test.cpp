
#include <string>
#include <iostream>

#include "src/ORM.h"

class MyClass
{
public:
	int id;
	double real;
	std::string str;

	ORHOOK (id, real, str)
};

class MyClass2
{
public:
	int id, age;
	double score;
	std::string firstName, lastName;

	ORHOOK (id, age, score, firstName, lastName)
};

int main ()
{
	using namespace BOT_ORM;

	ORMapper<MyClass> mapper ("test.db", "tbl");
	auto fnQuery1 = [&] ()
	{
		std::vector<MyClass> dbState;
		mapper.Query (dbState, "");
		for (const auto &i : dbState)
			std::cout << i.id << " " << i.real << " "
			<< i.str << std::endl;
		std::cout << std::endl;
	};
	mapper.CreateTbl ();

	mapper.Insert (MyClass { 1, 0.2, "John" });
	mapper.Insert (MyClass { 2, 0.4, "Jack" });
	mapper.Insert (MyClass { 2, 0.4, "Jack" });
	mapper.Insert (MyClass { 3, 0.8, "Hack" });
	fnQuery1 ();

	mapper.Update (MyClass { 2, 0.6, "Jack" });
	fnQuery1 ();

	mapper.Delete (MyClass { 2, 0.4, "Jack" });
	fnQuery1 ();

	ORMapper<MyClass2> mapper2 ("test.db", "tbl2");
	auto fnQuery2 = [&] ()
	{
		std::vector<MyClass2> dbState;
		mapper2.Query (dbState, "");
		for (const auto &i : dbState)
			std::cout << i.id << " " << i.age << " "
			<< i.score << " " << i.firstName << " "
			<< i.lastName << std::endl;
		std::cout << std::endl;
	};
	mapper2.CreateTbl ();
	mapper2.Insert (MyClass2 { 1, 20, 0.2, "John", "Lee" });
	mapper2.Insert (MyClass2 { 2, 37, 0.4, "Jack", "Lee" });
	fnQuery2 ();
	mapper2.Delete (MyClass2 { 2, 37, 0.4, "Jack", "Lee" });
	fnQuery2 ();

	getchar ();
	return 0;
}
