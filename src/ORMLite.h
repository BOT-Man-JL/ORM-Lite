// ORM Lite
// An ORM for SQLite in C++11
// https://github.com/BOT-Man-JL/ORM-Lite
// BOT Man, 2016

#ifndef BOT_ORM_H
#define BOT_ORM_H

#include <functional>
#include <vector>
#include <string>
#include <cctype>
#include <algorithm>
#include <thread>

#include "sqlite3.h"

#define ORMAP(_MY_CLASS_, ...)                        \
friend class BOT_ORM::ORMapper<_MY_CLASS_>;           \
void __Accept (BOT_ORM_Impl::ORVisitor &visitor)      \
{                                                     \
	visitor.Visit (__VA_ARGS__);                      \
}                                                     \
std::string __ClassName () const                      \
{                                                     \
	return #_MY_CLASS_;                               \
}                                                     \
std::string __FieldNames () const                     \
{                                                     \
	return #__VA_ARGS__;                              \
}

namespace BOT_ORM_Impl
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

		~SQLConnector ()
		{
			sqlite3_close (db);
		}

		// Don't throw in callback
		void Excute (const std::string &cmd,
					 std::function<void (int argc, char **argv,
										 char **azColName) noexcept>
					 callback = _callback)
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

	private:
		sqlite3 *db;
		static void _callback (int argc, char **argv, char **azColName)
		{ return; }
	};

	// Helper Funcion
	std::string SplitStr (std::string &inputStr,
						  char chSeg = char (0))
	{
		size_t pos = 0;
		if ((pos = inputStr.find (chSeg, 0)) != std::string::npos)
		{
			auto ret = inputStr.substr (0, pos);
			inputStr.erase (0, pos + 1);
			return std::move (ret);
		}
		return std::string ();
	}

	class ORVisitor
	{
	public:
		template <typename... Args>
		inline void Visit (Args & ... args)
		{
			return _Visit (args...);
		}

	protected:
		template <typename T, typename... Args>
		inline void _Visit (T &property, Args & ... args)
		{
			_Visit (property);
			_Visit (args...);
		}

		template <typename T>
		inline void _Visit (T &property)
		{
			// If you want to Use other Types, please Convert first...
			static_assert (false,
						   "Only Support long, double, std::string :-(");
		}

		virtual void _Visit (long &property) = 0;
		virtual void _Visit (double &property) = 0;
		virtual void _Visit (std::string &property) = 0;
	};

	class ReaderVisitor : public ORVisitor
	{
	public:
		std::string serializedValues;

	protected:
		void _Visit (long &property) override
		{
			serializedValues += std::to_string (property);
			serializedValues += char (0);
		}
		void _Visit (double &property) override
		{
			serializedValues += std::to_string (property);
			serializedValues += char (0);
		}
		void _Visit (std::string &property) override
		{
			serializedValues += "'" + property + "'";
			serializedValues += char (0);
		}
	};

	class TypeVisitor : public ORVisitor
	{
	public:
		std::string serializedTypes;

	protected:
		void _Visit (long &property) override
		{
			serializedTypes += "integer";
			serializedTypes += char (0);
		}
		void _Visit (double &property) override
		{
			serializedTypes += "real";
			serializedTypes += char (0);
		}
		void _Visit (std::string &property) override
		{
			serializedTypes += "text";
			serializedTypes += char (0);
		}
	};

	class WriterVisitor : public ORVisitor
	{
		std::string _serializedValues;

	public:
		WriterVisitor (std::string serializedValues)
			: _serializedValues (serializedValues)
		{}

	protected:
		void _Visit (long &property) override
		{
			property = std::stol (
				BOT_ORM_Impl::SplitStr (_serializedValues));
		}
		void _Visit (double &property) override
		{
			property = std::stod (
				BOT_ORM_Impl::SplitStr (_serializedValues));
		}
		void _Visit (std::string &property) override
		{
			property = BOT_ORM_Impl::SplitStr (_serializedValues);
		}
	};

}

namespace BOT_ORM
{
	template <typename C>
	class ORMapper
	{
	public:
		ORMapper (const std::string &dbName)
			: _dbName (dbName),
			_tblName (C ().__ClassName ())
		{}

		inline const std::string &ErrMsg () const
		{
			return _errMsg;
		}

		bool CreateTbl ()
		{
			return HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				C cl;
				BOT_ORM_Impl::TypeVisitor visitor;
				cl.__Accept (visitor);

				auto strTypes = std::move (visitor.serializedTypes);
				auto strFieldNames = ExtractFieldName (cl);

				auto typeFmt = BOT_ORM_Impl::SplitStr (strTypes);
				auto fieldName = BOT_ORM_Impl::SplitStr (strFieldNames);
				auto strFmt = std::move (fieldName) + " " +
					std::move (typeFmt) + " primary key,";

				while (!strTypes.empty ())
				{
					typeFmt = BOT_ORM_Impl::SplitStr (strTypes);
					fieldName = BOT_ORM_Impl::SplitStr (strFieldNames);
					strFmt += std::move (fieldName) + " " +
						std::move (typeFmt) + ",";
				}
				strFmt.pop_back ();

				connector.Excute ("create table " + _tblName +
								  "(" + strFmt + ");");
			});
		}

		bool DropTbl ()
		{
			return HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				connector.Excute ("drop table " + _tblName + ";");
			});
		}

		bool Insert (C &value)
		{
			return HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				BOT_ORM_Impl::ReaderVisitor visitor;
				value.__Accept (visitor);
				auto strIns = std::move (visitor.serializedValues);

				std::for_each (strIns.begin (), strIns.end (),
							   [] (auto &ch)
				{
					if (ch == char (0)) ch = ',';
				});
				strIns.pop_back ();

				connector.Excute ("insert into " + _tblName +
								  " values (" + strIns + ");");
			});
		}

		bool Delete (const std::string &cond)
		{
			return HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				connector.Excute ("delete from " + _tblName +
								  " " + cond + ";");
			});
		}

		bool Delete (C &value)
		{
			return HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				BOT_ORM_Impl::ReaderVisitor visitor;
				value.__Accept (visitor);

				auto strVals = std::move (visitor.serializedValues);
				auto strFieldNames = ExtractFieldName (value);

				// Only Set Key
				auto val = BOT_ORM_Impl::SplitStr (strVals);
				auto fieldName = BOT_ORM_Impl::SplitStr (strFieldNames);
				auto strDel =
					std::move (fieldName) + "=" + std::move (val);

				connector.Excute ("delete from " + _tblName +
								  " where " + strDel + ";");
			});
		}

		bool Update (C &value)
		{
			return HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				BOT_ORM_Impl::ReaderVisitor visitor;
				value.__Accept (visitor);

				auto strVals = std::move (visitor.serializedValues);
				auto strFieldNames = ExtractFieldName (value);

				std::string strKey;
				{
					auto val = BOT_ORM_Impl::SplitStr (strVals);
					auto fieldName = BOT_ORM_Impl::SplitStr (strFieldNames);
					strKey = std::move (fieldName) + "=" + std::move (val);
				}

				std::string strUpd;
				strUpd.reserve (2 * strVals.size () + strFieldNames.size ());
				while (!strVals.empty ())
				{
					auto val = BOT_ORM_Impl::SplitStr (strVals);
					auto fieldName = BOT_ORM_Impl::SplitStr (strFieldNames);
					strUpd += std::move (fieldName) + "=" + std::move (val) + ",";
				}
				strUpd.pop_back ();

				connector.Excute ("update " + _tblName +
								  " set " + strUpd +
								  " where " + strKey + ";");
			});
		}

		bool Select (std::vector<C> &out,
					const std::string &cond = "")
		{
			return HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				connector.Excute ("select * from " + _tblName +
								  " " + cond + ";",
								  [&] (int argc, char **argv, char **)
				{
					std::string serialized;
					for (size_t i = 0; i < argc; i++)
					{
						serialized += argv[i];
						serialized += char (0);
					}
					C obj;
					obj.__Accept (BOT_ORM_Impl::WriterVisitor (serialized));
					out.push_back (std::move (obj));
				});
			});
		}

		size_t Count (const std::string &cond = "")
		{
			auto ret = 0;
			HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				connector.Excute ("select count (*) as num from " +
								  _tblName + " " + cond,
								  [&] (int, char **argv, char **)
				{
					ret = std::stoi (argv[0]);
				});
			});
			return ret;
		}

	private:
		std::string _dbName, _tblName;
		std::string _errMsg;

		bool HandleException (std::function <void (
			BOT_ORM_Impl::SQLConnector &)> fn)
		{
			try
			{
				BOT_ORM_Impl::SQLConnector connector (_dbName);
				fn (connector);
				return true;
			}
			catch (const std::exception &ex)
			{
				_errMsg = ex.what ();
				return false;
			}
		}

		template <typename C>
		static std::string ExtractFieldName (const C& obj)
		{
			std::string ret;
			auto rawStr = obj.__FieldNames ();
			ret.reserve (rawStr.size ());
			for (const auto &ch : rawStr)
			{
				switch (ch)
				{
				case ',':
					ret += char (0);
					break;

				case '_':
				default:
					if (isalnum (ch) || isalpha (ch))
						ret += ch;
					break;
				}
			}
			ret += char (0);
			return std::move (ret);
		}
	};
}

#endif // !BOT_ORM_H