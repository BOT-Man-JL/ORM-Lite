
// A Sample of ORM Lite
// https://github.com/BOT-Man-JL/ORM-Lite
// BOT Man, 2016

#include <string>
#include <iostream>

#include "src/ORMLite.h"
using namespace BOT_ORM;
using namespace BOT_ORM::FieldHelper;
using namespace BOT_ORM::AggregateHelper;

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

	// Create tables
	try
	{
		mapper.CreateTbl (UserModel {});
	}
	catch (...)
	{
		mapper.DropTbl (UserModel {});
		mapper.CreateTbl (UserModel {});
	}
	try
	{
		mapper.CreateTbl (SellerModel {});
	}
	catch (...)
	{
		mapper.DropTbl (SellerModel {});
		mapper.CreateTbl (SellerModel {});
	}
	try
	{
		mapper.CreateTbl (OrderModel {});
	}
	catch (...)
	{
		mapper.DropTbl (OrderModel {});
		mapper.CreateTbl (OrderModel {});
	}

	/* #1 Basic Usage */

	std::vector<UserModel> initObjs =
	{
		{ 0, "John", 0.2, 21, nullptr, nullptr },
		{ 1, "Jack", 0.4, nullptr, 3.14, nullptr },
		{ 2, "Jess", 0.6, nullptr, nullptr, std::string ("Dr.") }
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
			UserModel { i, "July-" + std::to_string (i), i * 0.2 });

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

	// Define a Query Helper Object and its Field Extractor
	UserModel helper;
	auto field = Field (helper);

	// Select by Query :-)
	auto result2 = mapper.Query (UserModel {})
		.Where (
			field (helper.user_name) & std::string ("July%") &&
			(field (helper.age) >= 35 && field (helper.title) != nullptr)
		)
		.OrderByDescending (field (helper.user_id))
		.Take (3)
		.Skip (1)
		.ToVector ();

	// Remarks:
	// sql = SELECT * FROM UserModel
	//       WHERE ((name='July' AND (age>=35 AND title IS NOT NULL)))
	//       ORDER BY id DESC
	//       LIMIT 3 OFFSET 1
	// result2 = [{ 88, 17.6, "July", 38, null, "Mr. 18" },
	//            { 87, 17.4, "July", 37, null, "Mr. 17" },
	//            { 86, 17.2, "July", 36, null, "Mr. 16" }]

	// Reusable Query Object :-)
	auto query = mapper.Query (UserModel {})
		.Where (field (helper.user_name) & std::string ("July%"));

	// Aggregate Function by Query :-)
	auto count = query.Aggregate (Count ());
	auto avg = query.Aggregate (Avg (field (helper.credit_count)));

	// Remarks:
	// sql = SELECT COUNT (*) FROM UserModel
	//       WHERE (name='July')
	// count = 50

	// Update by Condition :-)
	mapper.Update (UserModel {},
				   field (helper.user_name) == std::string ("July"),
				   field (helper.age) = 10,
				   field (helper.credit_count) = 1.0);

	// Remarks:
	// sql = UPDATE UserModel SET score=10,credit_count=1.0
	//       WHERE (name='July')

	// Delete by Condition :-)
	mapper.Delete (UserModel {},
				   field (helper.user_id) >= 90);

	// Remarks:
	// sql = DELETE FROM UserModel WHERE (id>=90)

	/* #4 Multi Table */

	UserModel user;
	auto userField = Field (user);
	SellerModel seller;
	auto sellerField = Field (seller);
	OrderModel order;
	auto orderField = Field (order);

	mapper.Transaction ([&] ()
	{
		for (size_t i = 0; i < 50; i++)
			mapper.Insert (
				OrderModel { 0,
				(int) i / 2 + 50,
				(int) i / 4 + 50,
				"Item " + std::to_string (i), i * 0.5 },
				false);
	});

	auto joined = mapper.Query (UserModel {})
		.Join (OrderModel {},
			   userField (user.user_id) == orderField (order.user_id))
		.LeftJoin (SellerModel {},
			   sellerField (seller.seller_id) == orderField (order.seller_id))
		.Where (userField (user.user_id) >= 65);

	auto sum = joined.Aggregate (Count ());

	auto result3 = joined
		.Select (userField (user.user_name),
				 Sum (orderField (order.fee)))
		.GroupBy (userField (user.user_name))
		.ToList ();

	// ==========

	// Drop tables
	mapper.DropTbl (UserModel {});
	mapper.DropTbl (OrderModel {});

	// Output Nullable Field
	auto printNullable = [] (std::ostream &os, const auto &val)
		-> std::ostream &
	{
		if (val == nullptr)
			return os << "null";
		else
			return os << val.Value ();
	};

	// Output UserModel Objects
	auto printUserModeles = [&printNullable] (const auto &objs)
	{
		for (auto& item : objs)
		{
			std::cout << item.user_id << "\t" << item.credit_count
				<< "\t" << item.user_name << "\t";
			printNullable (std::cout, item.age) << "\t";
			printNullable (std::cout, item.salary) << "\t";
			printNullable (std::cout, item.title) << std::endl;
		}
		std::cout << std::endl;
	};

	// Output Tuple Objects
	auto printTuples = [&printNullable] (auto &objs, size_t size)
	{
		for (auto &entry : objs)
		{
			std::cout << "(";
			size_t index = 0;
			BOT_ORM_Impl::TupleVisitor (
				entry, [&index, &size, &printNullable] (const auto &val)
			{
				printNullable (std::cout, val);
				if (++index != size) std::cout << ",";
			});
			std::cout << ")\n";
		}
		std::cout << std::endl;
	};

	// Sec 1
	printUserModeles (result1);

	// Sec 2
	printUserModeles (result2);
	printNullable (std::cout, count) << std::endl;
	printNullable (std::cout, avg) << std::endl;
	std::cout << std::endl;

	// Sec 3
	printTuples (result3,
				 std::tuple_size<decltype (result3)::value_type>::value);
	printNullable (std::cout, sum) << std::endl;

	std::cin.get ();
	return 0;
}
