#ifndef BOT_SQL_H
#define BOT_SQL_H

#include <string>
#include <functional>
#include <thread>
#include <exception>
#include <strstream>
#include "./SQLite/sqlite3.h"

namespace BOT_SQL
{
	class SQLConnector
	{
	public:
		SQLConnector (const std::string &fileName)
		{
			auto rc = sqlite3_open (fileName.c_str (), &db);
			if (rc)
			{
				sqlite3_close (db);
				throw std::runtime_error (
					std::string ("Can't open database: %s\n")
					+ sqlite3_errmsg (db));
			}
		}

		// Don't throw in callback
		void Excute (const std::string &cmd,
					 std::function<void (int argc, char **argv,
										 char **azColName) noexcept>
					 callback)
		{
			const size_t MAX_TRIAL = 16;
			char *zErrMsg = 0;
			int rc;

			auto callbackWrapper = [] (void *fn, int argc, char **argv,
									   char **azColName)
			{
				static_cast<std::function
					<void (int argc,
						   char **argv,
						   char **azColName) noexcept> *>
						   (fn)->operator()(argc, argv, azColName);
				return 0;
			};

			for (size_t iTry = 0; iTry < MAX_TRIAL; iTry++)
			{
				rc = sqlite3_exec (db, cmd.c_str (), callbackWrapper,
								   &callback, &zErrMsg);
				if (rc != SQLITE_BUSY)
					break;

				std::this_thread::sleep_for (std::chrono::microseconds (20));
			}

			if (rc != SQLITE_OK)
			{
				auto errStr = std::string ("SQL error: ") + zErrMsg;
				sqlite3_free (zErrMsg);
				throw std::runtime_error (errStr);
			}
		}

		void CreateTable (const std::string &tableName,
						  const std::string &params)
		{
			auto cmd = "create table " + tableName +
				"(" + params + ");";
			Excute (cmd, _callback);
		}

		void DropTable (const std::string &tableName)
		{
			auto cmd = "drop table " + tableName + ";";
			Excute (cmd, _callback);
		}

		void InsertValue (const std::string &tableName,
						  const std::string &value)
		{
			auto cmd = "insert into " + tableName +
				" values (" + value + ");";
			Excute (cmd, _callback);
		}

		void DeleteValue (const std::string &tableName,
						  const std::string &where = "")
		{
			std::string cmd;
			if (where.size ())
				cmd = "delete from " + tableName +
				" where (" + where + ");";
			else
				cmd = "delete * from " + tableName + ";";
			Excute (cmd, _callback);
		}

		void UpdateValue (const std::string &tableName,
						  const std::string &set,
						  const std::string &where)
		{
			std::string cmd =
				"update " + tableName + " set " + set +
				" where (" + where + ");";
			Excute (cmd, _callback);
		}

		size_t CountValue (const std::string &tableName,
						   const std::string &select,
						   const std::string &where = "")
		{
			std::string cmd;
			if (where.size ())
				cmd = "select count (" + select + ") as num from " + tableName +
				" where (" + where + ");";
			else
				cmd = "select count (" + select + ") as num from " + tableName + ";";

			auto ret = 0;
			auto callback = [&] (int argc, char **argv,
								 char **azColName)
			{
				std::istrstream strs (argv[0]);
				strs >> ret;
			};
			Excute (cmd, callback);
			return ret;
		}

		void QueryValue (const std::string &tableName,
						 const std::string &select,
						 const std::string &where,
						 std::function<void (int argc, char **argv,
											 char **azColName) noexcept>
						 callback)
		{
			std::string cmd;
			if (where.size ())
				cmd = "select " + select + " from " + tableName +
				" where (" + where + ");";
			else
				cmd = "select " + select + " from " + tableName + ";";
			Excute (cmd, std::move (callback));
		}

		~SQLConnector ()
		{
			sqlite3_close (db);
		}

	private:
		sqlite3 *db;
		static void _callback (int argc, char **argv, char **azColName)
		{ return; }
	};
}

// Helper Fucntions

namespace BOT_SQL_Helper
{
	// Table Format

	std::string _sql_tbl_fmt ()
	{
		return "";
	}

	template <typename... Args>
	std::string _sql_tbl_fmt (
		std::pair<std::string, std::string> arg1,
		Args ... args)
	{
		return "," + std::move (arg1.first) +
			' ' + std::move (arg1.second) +
			_sql_tbl_fmt (args...);
	}

	template <typename... Args>
	std::string sql_tbl_fmt (
		std::pair<std::string, std::string> arg1,
		bool isPrimaryKey,
		Args ... args)
	{
		auto fmt = std::move (arg1.first) +
			' ' + std::move (arg1.second);
		if (isPrimaryKey)
			fmt += " primary key";
		return std::move (fmt) + _sql_tbl_fmt (args...);
	}

	// Where

	template <typename T, typename Q>
	std::string sql_where (
		const std::pair<std::string, std::string> &field,
		const Q &op,
		const T &value)
	{
		static_assert (std::is_arithmetic<T>::value, "T must be arithmetic");
		return field.first + op + std::to_string (value);
	}

	template <typename Q>
	inline std::string sql_where (
		const std::pair<std::string, std::string> &field,
		const Q &op,
		const std::string &value)
	{
		return field.first + op + "'" + value + "'";
	}

	template <typename Q>
	inline std::string sql_where (
		const std::pair<std::string, std::string> &field,
		const Q &op,
		const char *value)
	{
		return sql_where (field, op, std::string (value));
	}

	// Insert

	template <typename T>
	std::string sql_insert (const T &value)
	{
		static_assert (std::is_arithmetic<T>::value, "T must be arithmetic");
		return std::to_string (value);
	}

	inline std::string sql_insert (const std::string &value)
	{
		return "'" + value + "'";
	}

	inline std::string sql_insert (const char *value)
	{
		return sql_insert (std::string (value));
	}

	template <typename T, typename... Args>
	std::string sql_insert (const T &value,
							const Args & ... args)
	{
		static_assert (std::is_arithmetic<T>::value, "T must be arithmetic");
		return std::to_string (value) + "," + sql_insert (args...);
	}

	template <typename... Args>
	inline std::string sql_insert (const std::string &value,
								   const Args & ... args)
	{
		return "'" + value + "'," + sql_insert (args...);
	}

	template <typename... Args>
	inline std::string sql_insert (const char *value,
								   const Args & ... args)
	{
		return sql_insert (std::string (value), args...);
	}
}

#endif // !BOT_SQL_H