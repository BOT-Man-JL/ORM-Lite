
// ORM Lite
// An ORM for SQLite in C++11
// https://github.com/BOT-Man-JL/ORM-Lite
// BOT Man, 2016

#ifndef BOT_ORM_H
#define BOT_ORM_H

#include <functional>
#include <vector>
#include <list>
#include <string>
#include <cctype>
#include <thread>
#include <strstream>

#include "sqlite3.h"

#define ORMAP(_MY_CLASS_, ...)                            \
friend class BOT_ORM::ORMapper<_MY_CLASS_>;               \
void __Accept (BOT_ORM_Impl::ORVisitor &visitor)          \
{                                                         \
	visitor.Visit (__VA_ARGS__);                          \
}                                                         \
void __Accept (BOT_ORM_Impl::ORVisitor &visitor) const   \
{                                                         \
	visitor.Visit (__VA_ARGS__);                          \
}                                                         \
static constexpr char *__ClassName = #_MY_CLASS_;         \
static constexpr char *__FieldNames = #__VA_ARGS__;       \

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
		void Execute (const std::string &cmd,
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

	std::string DoubleToStr (double val)
	{
		std::string ret;
		std::strstream strs;
		strs << val;
		strs >> ret;
		return std::move (ret);
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
		virtual void _Visit (const long &property) = 0;

		virtual void _Visit (double &property) = 0;
		virtual void _Visit (const double &property) = 0;

		virtual void _Visit (std::string &property) = 0;
		virtual void _Visit (const std::string &property) = 0;
	};

	class ReaderVisitor : public ORVisitor
	{
	public:
		std::string serializedValues;

	protected:
		void _Visit (const long &property) override final
		{
			serializedValues += std::to_string (property);
			serializedValues += char (0);
		}
		void _Visit (const double &property) override final
		{
			serializedValues +=
				BOT_ORM_Impl::DoubleToStr (property);
			serializedValues += char (0);
		}
		void _Visit (const std::string &property) override final
		{
			serializedValues += "'" + property + "'";
			serializedValues += char (0);
		}

	private:
		void _Visit (long &property) override final {}
		void _Visit (double &property) override final {}
		void _Visit (std::string &property) override final {}
	};

	class TypeVisitor : public ORVisitor
	{
	public:
		std::string serializedTypes;

	protected:
		void _Visit (const long &property) override final
		{
			serializedTypes += "integer";
			serializedTypes += char (0);
		}
		void _Visit (const double &property) override final
		{
			serializedTypes += "real";
			serializedTypes += char (0);
		}
		void _Visit (const std::string &property) override final
		{
			serializedTypes += "text";
			serializedTypes += char (0);
		}

	private:
		void _Visit (long &property) override final {}
		void _Visit (double &property) override final {}
		void _Visit (std::string &property) override final {}
	};

	class WriterVisitor : public ORVisitor
	{
		std::string _serializedValues;

	public:
		WriterVisitor (std::string serializedValues)
			: _serializedValues (serializedValues)
		{}

	protected:
		void _Visit (long &property) override final
		{
			property = std::stol (
				BOT_ORM_Impl::SplitStr (_serializedValues));
		}
		void _Visit (double &property) override final
		{
			property = std::stod (
				BOT_ORM_Impl::SplitStr (_serializedValues));
		}
		void _Visit (std::string &property) override final
		{
			property = BOT_ORM_Impl::SplitStr (_serializedValues);
		}

	private:
		void _Visit (const long &property) override final {}
		void _Visit (const double &property) override final {}
		void _Visit (const std::string &property) override final {}
	};

	class IndexVisitor : public ORVisitor
	{
		const void *_pointer;

	public:
		bool isFound;
		size_t index;

		IndexVisitor (const void *pointer)
			: index (0), isFound (false),
			_pointer (pointer)
		{}

	protected:
		void _Visit (const long &property) override final
		{
			if ((const void *) &property == _pointer)
				isFound = true;
			else if (!isFound)
				index++;
		}
		void _Visit (const double &property) override final
		{
			if ((const void *) &property == _pointer)
				isFound = true;
			else if (!isFound)
				index++;
		}
		void _Visit (const std::string &property) override final
		{
			if ((const void *) &property == _pointer)
				isFound = true;
			else if (!isFound)
				index++;
		}

	private:
		void _Visit (long &property) override final {}
		void _Visit (double &property) override final {}
		void _Visit (std::string &property) override final {}
	};
}

namespace BOT_ORM
{
	struct Expr
	{
		std::vector<std::pair<const void *, std::string>> expr;

		template <typename T>
		Expr (const T &property,
			  const std::string &relOp = "=")
			: Expr (property, relOp, property)
		{}

		Expr (const long &property,
			  const std::string &relOp,
			  long value)
			: expr { std::make_pair (
				&property,
				relOp + std::to_string (value)) }
		{}

		Expr (const double &property,
			  const std::string &relOp,
			  double value)
			: expr { std::make_pair (
				&property,
				relOp + BOT_ORM_Impl::DoubleToStr (value)) }
		{}

		Expr (const std::string &property,
			  const std::string &relOp,
			  std::string value)
			: expr { std::make_pair (
				&property,
				relOp + "'" + std::move (value) + "'") }
		{}

		inline Expr operator && (const Expr &right)
		{
			return And_Or (right, " and ");
		}

		inline Expr operator || (const Expr &right)
		{
			return And_Or (right, " or ");
		}

	private:
		Expr And_Or (const Expr &right, std::string logOp)
		{
			constexpr void *ptr = nullptr;

			expr.emplace (
				expr.begin (),
				std::make_pair (ptr, std::string ("("))
			);
			expr.emplace_back (
				std::make_pair (ptr, std::move (logOp))
			);
			for (const auto exprPair : right.expr)
				expr.emplace_back (exprPair);
			expr.emplace_back (
				std::make_pair (ptr, std::string (")"))
			);

			return *this;
		}
	};

	template <typename C>
	class ORMapper
	{
	public:
		ORMapper (const std::string &dbName)
			: _dbName (dbName),
			_tblName (C::__ClassName)
		{}

		inline const std::string &ErrMsg () const
		{
			return _errMsg;
		}

		bool CreateTbl ()
		{
			return _HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				const C obj;
				BOT_ORM_Impl::TypeVisitor visitor;
				obj.__Accept (visitor);

				auto strTypes = std::move (visitor.serializedTypes);
				auto strFieldNames = _ExtractFieldName ();

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

				connector.Execute ("create table " + _tblName +
								   "(" + strFmt + ");");
			});
		}

		bool DropTbl ()
		{
			return _HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				connector.Execute ("drop table " + _tblName + ";");
			});
		}

		bool Insert (const C &value)
		{
			return _HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				BOT_ORM_Impl::ReaderVisitor visitor;
				value.__Accept (visitor);
				auto strIns = std::move (visitor.serializedValues);

				for (auto &ch : strIns)
					if (ch == char (0))
						ch = ',';
				strIns.pop_back ();

				connector.Execute ("insert into " + _tblName +
								   " values (" + strIns + ");");
			});
		}

		template <typename In>
		bool Insert (const In &values)
		{
			return _HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				std::string strIns;
				for (const auto &value : values)
				{
					BOT_ORM_Impl::ReaderVisitor visitor;
					value.__Accept (visitor);
					auto strValues = std::move (visitor.serializedValues);

					for (auto &ch : strValues)
						if (ch == char (0))
							ch = ',';
					strValues.pop_back ();
					strIns += "(" + strValues + "),";
				}
				strIns.pop_back ();
				connector.Execute ("insert into " + _tblName +
								   " values " + strIns + ";");
			});
		}

		bool Delete (const C &value)
		{
			return _HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				BOT_ORM_Impl::ReaderVisitor visitor;
				value.__Accept (visitor);

				auto strVals = std::move (visitor.serializedValues);
				auto strFieldNames = _ExtractFieldName ();

				// Only Set Key
				auto val = BOT_ORM_Impl::SplitStr (strVals);
				auto fieldName = BOT_ORM_Impl::SplitStr (strFieldNames);
				auto strDel =
					std::move (fieldName) + "=" + std::move (val);

				connector.Execute ("delete from " + _tblName +
								   " where " + strDel + ";");
			});
		}

		bool Delete (const std::string &sqlStr)
		{
			return _HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				connector.Execute ("delete from " + _tblName +
								   " " + sqlStr + ";");
			});
		}

		bool Update (const C &value)
		{
			return _HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				BOT_ORM_Impl::ReaderVisitor visitor;
				value.__Accept (visitor);

				auto strVals = std::move (visitor.serializedValues);
				auto strFieldNames = _ExtractFieldName ();

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

				connector.Execute ("update " + _tblName +
								   " set " + std::move (strUpd) +
								   " where " + std::move (strKey) + ";");
			});
		}

		template <typename In>
		bool Update (const In &values)
		{
			return _HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				auto strFieldNames = _ExtractFieldName ();
				std::vector<std::string> fieldNames;
				while (!strFieldNames.empty ())
					fieldNames.emplace_back (BOT_ORM_Impl::SplitStr (strFieldNames));

				std::string strUpdate;
				for (const auto &value : values)
				{
					BOT_ORM_Impl::ReaderVisitor visitor;
					value.__Accept (visitor);

					auto strVals = std::move (visitor.serializedValues);
					auto curIndex = 0;

					std::string strKey = fieldNames[curIndex++] +
						"=" + BOT_ORM_Impl::SplitStr (strVals);

					std::string strUpd;
					while (!strVals.empty ())
						strUpd += fieldNames[curIndex++] + "=" +
						BOT_ORM_Impl::SplitStr (strVals) + ",";
					strUpd.pop_back ();

					strUpdate += "update " + _tblName +
						" set " + std::move (strUpd) +
						" where " + std::move (strKey) + ";";
				}
				connector.Execute (strUpdate);
			});
		}

		template <typename Out>
		bool Select (Out &out,
					 const std::string &sqlStr = "")
		{
			return _HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				connector.Execute ("select * from " + _tblName +
								   " " + sqlStr + ";",
								   [&] (int argc, char **argv, char **)
				{
					std::string serialized;
					for (int i = 0; i < argc; i++)
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

		long Count (const std::string &sqlStr = "")
		{
			long ret;
			auto isOk = _HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				connector.Execute ("select count (*) as num from " +
								   _tblName + " " + sqlStr,
								   [&] (int, char **argv, char **)
				{
					ret = std::stoi (argv[0]);
				});
			});
			if (isOk)
				return ret;
			else
				return -1;
		}

		class ORQuery
		{
		public:
			ORQuery (const C &queryHelper, ORMapper<C> *pMapper)
				: _qObj (&queryHelper), _pMapper (pMapper)
			{}

			// Where
			ORQuery &Where (const Expr &expr)
			{
				for (const auto exprStr : expr.expr)
				{
					if (exprStr.first != nullptr)
						_sqlWhere += _GetFieldName (exprStr.first);
					_sqlWhere += exprStr.second;
				}
				return *this;
			}

			// Order By
			template <typename T>
			ORQuery &OrderBy (const T &property,
							  bool isDecreasing = false)
			{
				_sqlOrderBy = " order by " + _GetFieldName (&property);
				if (isDecreasing)
					_sqlOrderBy += " desc";
				return *this;
			}

			// Limit
			ORQuery &Limit (size_t count, size_t offset = 0)
			{
				_sqlLimit = " limit " + std::to_string (count) +
					" offset " + std::to_string (offset);
				return *this;
			}

			std::string GetSQL () const
			{
				if (!_sqlWhere.empty ())
					return " where (" + _sqlWhere + ")" + _sqlOrderBy + _sqlLimit;
				else
					return _sqlOrderBy + _sqlLimit;
			}

			// Retrieve Select Result
			std::vector<C> ToVector ()
			{
				std::vector<C> ret;
				_pMapper->Select (ret, GetSQL ());
				return std::move (ret);
			}

			std::list<C> ToList ()
			{
				std::list<C> ret;
				_pMapper->Select (ret, GetSQL ());
				return std::move (ret);
			}

			// Count Result
			inline long Count ()
			{
				return _pMapper->Count (" where (" + _sqlWhere + ")");
			}

			// Delete Values
			inline bool Delete ()
			{
				return _pMapper->Delete (" where (" + _sqlWhere + ")");
			}

		protected:
			const C *_qObj;
			ORMapper<C> *_pMapper;
			std::string _sqlWhere;
			std::string _sqlOrderBy, _sqlLimit;

			std::string _GetFieldName (const void *property)
			{
				BOT_ORM_Impl::IndexVisitor visitor (property);
				(*_qObj).__Accept (visitor);

				if (!visitor.isFound)
					throw std::runtime_error ("No such Field in the Table");

				auto strFieldNames = _ExtractFieldName ();
				auto fieldName = BOT_ORM_Impl::SplitStr (strFieldNames);
				for (auto index = visitor.index; index > 0; index--)
				{
					fieldName = BOT_ORM_Impl::SplitStr (strFieldNames);
				}
				return std::move (fieldName);
			}
		};

		ORQuery Query (const C &queryHelper)
		{
			return ORQuery (queryHelper, this);
		}

	private:
		std::string _dbName, _tblName;
		std::string _errMsg;

		bool _HandleException (std::function <void (
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

		static std::string _ExtractFieldName ()
		{
			std::string ret, rawStr (C::__FieldNames);
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