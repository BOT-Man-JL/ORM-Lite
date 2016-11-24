
// A Sample of ORM Lite
// https://github.com/BOT-Man-JL/ORM-Lite
// BOT Man, 2016

#include <string>
#include <iostream>
#include <tuple>

#include "src/ORMLite.h"
using namespace BOT_ORM;
using namespace BOT_ORM::Expression;
using namespace BOT_ORM::Helper;

namespace PrintHelper
{
	template<class T>
	void PrintNullable (const BOT_ORM::Nullable<T> &val)
	{
		if (val == nullptr)
			std::cout << "null";
		else
			std::cout << val.Value ();
	}

	template<class Tuple, std::size_t N>
	struct TuplePrinter
	{
		static void print (const Tuple& t)
		{
			TuplePrinter<Tuple, N - 1>::print (t);
			std::cout << ", ";
			PrintNullable (std::get<N - 1> (t));
		}
	};

	template<class Tuple>
	struct TuplePrinter<Tuple, 1>
	{
		static void print (const Tuple& t)
		{
			PrintNullable (std::get<0> (t));
		}
	};

	template<class... Args>
	void PrintTuple (const std::tuple<Args...>& t)
	{
		std::cout << "(";
		TuplePrinter<decltype(t), sizeof...(Args)>::print (t);
		std::cout << ")\n";
	}

	template<class Container>
	void PrintTuples (const Container &vals)
	{
		for (const auto &val : vals)
			PrintTuple (val);
		std::cout << std::endl;
	}
}

struct UserModel
{
	int user_id;
	std::string user_name;
	double credit_count;

	Nullable<int> age;
	Nullable<double> salary;
	Nullable<std::string> title;

	// Inject ORM-Lite into this Class :-)
	ORMAP ("UserModel", user_id, user_name, credit_count, age, salary, title);
};

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
	ORMAP ("OrderModel", order_id, user_id, seller_id, product_name, fee);
};

int main ()
{
	// Open a Connection with *test.db*
	ORMapper mapper ("test.db");

	// Create Brand New Tables
	auto initTable = [&mapper] (const auto &model)
	{
		try
		{
			mapper.CreateTbl (model);
		}
		catch (...)
		{
			mapper.DropTbl (model);
			mapper.CreateTbl (model);
		}
	};
	initTable (UserModel {});
	initTable (SellerModel {});
	initTable (OrderModel {});

	// ==========

	/* #1 Basic Usage */

	std::vector<UserModel> initObjs =
	{
		{ 0, "John", 0.2, 21, nullptr, nullptr },
		{ 1, "Jack", 0.4, nullptr, 3.14, nullptr },
		{ 2, "Jess", 0.6, nullptr, nullptr, std::string ("Dr.") }
	};

	// Insert Values into the table
	for (const auto &obj : initObjs)
		mapper.Insert (obj);

	// Update Entry by Primary Key
	initObjs[1].salary = nullptr;
	initObjs[1].title = "St.";
	mapper.Update (initObjs[1]);

	// Delete Entry by Primary Key
	mapper.Delete (initObjs[2]);

	// Transactional Statements
	try
	{
		mapper.Transaction ([&] ()
		{
			mapper.Delete (initObjs[0]);
			mapper.Insert (UserModel { 1, "Joke", 0 });
		});
	}
	catch (const std::exception &ex)
	{
		// If any statement Failed, throw an exception
		// "SQL error: UNIQUE constraint failed: UserModel.id"

		// Remarks:
		// mapper.Delete (initObjs[0]); will not applied :-)
	}

	// Select All to List
	auto result1 = mapper.Query (UserModel {}).ToList ();
	//   result1 = [{ 0, 0.2, "John", 21,   null, null  },
	//              { 1, 0.4, "Jack", null, null, "St." }]

	/* #2 Batch Operations */

	std::vector<UserModel> dataToSeed;
	for (int i = 50; i < 100; i++)
		dataToSeed.emplace_back (
			UserModel { i, "July_" + std::to_string (i), i * 0.2 });

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
		mapper.UpdateRange (dataToSeed);
	});

	/* #3 Composite Query */

	// Define a Query Helper Object and its Field Extractor
	UserModel helper;
	FieldExtractor field { helper };

	// Select by Query :-)
	auto result2 = mapper.Query (UserModel {})
		.Where (
			field (helper.user_name) & std::string ("July%") &&
			(field (helper.age) >= 32 &&
			 field (helper.title) != nullptr)
		)
		.OrderByDescending (field (helper.age))
		.OrderBy (field (helper.user_id))
		.Take (3)
		.Skip (1)
		.ToVector ();

	// Remarks:
	// sql = SELECT * FROM UserModel
	//       WHERE (name LIKE 'July%' AND
	//              (age>=32 AND title IS NOT NULL))
	//       ORDER BY age DESC
	//       ORDER BY id
	//       LIMIT 3 OFFSET 1
	// result2 = [{ 89, 17.8, "July_89", 34, null, "Mr. 19" },
	//            { 86, 17.2, "July_86", 33, null, "Mr. 16" },
	//            { 87, 17.4, "July_87", 33, null, "Mr. 17" }]

	// Calculate Aggregate Function by Query :-)
	auto avg = mapper.Query (UserModel {})
		.Where (field (helper.user_name) & std::string ("July%"))
		.Aggregate (Avg (field (helper.credit_count)));

	// Remarks:
	// sql = SELECT AVG (credit_count) FROM UserModel
	//       WHERE (name LIKE 'July')
	// avg = 14.9

	auto count = mapper.Query (UserModel {})
		.Where (field (helper.user_name) | std::string ("July%"))
		.Aggregate (Count ());

	// Remarks:
	// sql = SELECT COUNT (*) FROM UserModel
	//       WHERE (name NOT LIKE 'July')
	// count = 2

	// Update by Condition :-)
	//mapper.Update (UserModel {},
	//			   field (helper.age) = 10 &&
	//			   field (helper.credit_count) = 1.0,
	//			   field (helper.user_name) == std::string ("July"));

	// Remarks:
	// sql = UPDATE UserModel SET age=10,credit_count=1.0
	//       WHERE (name='July')

	// Delete by Condition :-)
	mapper.Delete (UserModel {},
				   field (helper.user_id) >= 90);

	// Remarks:
	// sql = DELETE FROM UserModel WHERE (id>=90)

	/* #4 Multi-Table Query */

	// Define more Query Helper Objects and their Field Extractor
	UserModel user;
	SellerModel seller;
	OrderModel order;
	field = FieldExtractor { user, seller, order };

	// Insert Values into the table
	// mapper.Insert (..., false) means Insert without Primary Key
	for (size_t i = 0; i < 50; i++)
		mapper.Insert (
			OrderModel { 0,
			(int) i / 2 + 50,
			(int) i / 4 + 50,
			"Item " + std::to_string (i),
			i * 0.5 }, false);

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
	// There is Join Called, so the Result are Nullable-Tuples
	auto result3 = joinedQuery.ToList ();

	// Remarks:
	// sql = SELECT * FROM UserModel
	//       JOIN OrderModel
	//       ON UserModel.user_id=OrderModel.user_id
	//       LEFT JOIN SellerModel
	//       ON SellerModel.seller_id=OrderModel.seller_id
	//       WHERE (UserModel.user_id>=65)
	// result3 = [(65, "July_65", 13, null, null, null,
	//             31, 65, 57, "Item 30", 15,
	//             null, null, null),
	//            (65, "July_65", 13, null, null, null,
	//             32, 65, 57, "Item 31", 15.5,
	//             null, null, null),
	//            ... ]

	// Group & Having ~
	// There is Select Called, so the Result are Nullable-Tuples
	auto result4 = joinedQuery
		.Select (field (order.user_id),
				 field (user.user_name),
				 Avg (field (order.fee)))
		.GroupBy (field (user.user_name))
		.Having (Sum (field (order.fee)) >= 40.5)
		.Skip (3)
		.ToList ();

	// Remarks:
	// sql = SELECT OrderModel.user_id,
	//              UserModel.user_name,
	//              AVG (OrderModel.fee)
	//       FROM UserModel
	//            JOIN OrderModel
	//            ON UserModel.user_id=OrderModel.user_id
	//            LEFT JOIN SellerModel
	//            ON SellerModel.seller_id=OrderModel.seller_id
	//       WHERE (UserModel.user_id>=65)
	//       GROUP BY UserModel.user_name
	//       HAVING SUM (OrderModel.fee)>=40.5
	//       LIMIT ~0 OFFSET 3
	// result4 = [(73, "July_73", 23.25),
	//            (74, "July_74", 24.25)]

	// ==========

	// Output UserModel Objects
	auto printUserModeles = [] (const auto &objs)
	{
		for (auto& item : objs)
		{
			std::cout << item.user_id << "\t" << item.credit_count
				<< "\t" << item.user_name << "\t";
			PrintHelper::PrintNullable (item.age);
			std::cout << "\t";
			PrintHelper::PrintNullable (item.salary);
			std::cout << "\t";
			PrintHelper::PrintNullable (item.title);
			std::cout << "\n";
		}
		std::cout << std::endl;
	};

	// Sec 1
	printUserModeles (result1);

	// Sec 2
	printUserModeles (result2);
	PrintHelper::PrintNullable (count);
	std::cout << "\n";
	PrintHelper::PrintNullable (avg);
	std::cout << "\n" << std::endl;

	// Sec 3
	PrintHelper::PrintTuples (result3);
	PrintHelper::PrintTuples (result4);

	std::cin.get ();
	return 0;
}
