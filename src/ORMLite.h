
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
#include <sstream>

#include "sqlite3.h"

#define ORMAP(_MY_CLASS_, ...)                            \
private:                                                  \
friend class BOT_ORM::ORMapper<_MY_CLASS_>;               \
template <typename VISITOR>                               \
void __Accept (VISITOR &visitor)                          \
{                                                         \
	visitor.Visit (__VA_ARGS__);                          \
}                                                         \
template <typename VISITOR>                               \
void __Accept (VISITOR &visitor) const                    \
{                                                         \
	visitor.Visit (__VA_ARGS__);                          \
}                                                         \
constexpr static const char *__ClassName = #_MY_CLASS_;   \
constexpr static const char *__FieldNames = #__VA_ARGS__; \

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
		std::stringstream strs;
		strs << val;
		strs >> ret;
		return std::move (ret);
	}

	class ReaderVisitor
	{
	public:
		std::string serializedValues;

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

		inline void _Visit (const long &property)
		{
			serializedValues += std::to_string (property);
			serializedValues += char (0);
		}
		inline void _Visit (const double &property)
		{
			serializedValues +=
				BOT_ORM_Impl::DoubleToStr (property);
			serializedValues += char (0);
		}
		inline void _Visit (const std::string &property)
		{
			serializedValues += "'" + property + "'";
			serializedValues += char (0);
		}
	};

	class TypeVisitor
	{
	public:
		std::string serializedTypes;

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

		inline void _Visit (const long &property)
		{
			serializedTypes += "integer";
			serializedTypes += char (0);
		}
		inline void _Visit (const double &property)
		{
			serializedTypes += "real";
			serializedTypes += char (0);
		}
		inline void _Visit (const std::string &property)
		{
			serializedTypes += "text";
			serializedTypes += char (0);
		}
	};

	class WriterVisitor
	{
		std::string _serializedValues;

	public:
		WriterVisitor (std::string serializedValues)
			: _serializedValues (serializedValues)
		{}

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

		inline void _Visit (long &property)
		{
			property = std::stol (
				BOT_ORM_Impl::SplitStr (_serializedValues));
		}
		inline void _Visit (double &property)
		{
			property = std::stod (
				BOT_ORM_Impl::SplitStr (_serializedValues));
		}
		inline void _Visit (std::string &property)
		{
			property = BOT_ORM_Impl::SplitStr (_serializedValues);
		}
	};

	class IndexVisitor
	{
		const void *_pointer;

	public:
		bool isFound;
		size_t index;

		IndexVisitor (const void *pointer)
			: index (0), isFound (false),
			_pointer (pointer)
		{}

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
			if (!isFound)
				_Visit (args...);
		}

		inline void _Visit (const long &property)
		{
			if ((const void *) &property == _pointer)
				isFound = true;
			else if (!isFound)
				index++;
		}
		inline void _Visit (const double &property)
		{
			if ((const void *) &property == _pointer)
				isFound = true;
			else if (!isFound)
				index++;
		}
		inline void _Visit (const std::string &property)
		{
			if ((const void *) &property == _pointer)
				isFound = true;
			else if (!isFound)
				index++;
		}
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

		template <typename T>
		struct Field_Expr
		{
			const T& _property;
			Field_Expr (const T &property)
				: _property (property)
			{}

			inline Expr operator == (T value)
			{ return Expr { _property, "=", std::move (value) }; }
			inline Expr operator != (T value)
			{ return Expr { _property, "!=", std::move (value) }; }

			inline Expr operator > (T value)
			{ return Expr { _property, ">", std::move (value) }; }
			inline Expr operator >= (T value)
			{ return Expr { _property, ">=", std::move (value) }; }

			inline Expr operator < (T value)
			{ return Expr { _property, "<", std::move (value) }; }
			inline Expr operator <= (T value)
			{ return Expr { _property, "<=", std::move (value) }; }
		};

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

	template <typename T>
	Expr::Field_Expr<T> Field (T &property)
	{
		return Expr::Field_Expr<T> { property };
	}

	template <typename C>
	class ORMapper
	{
	public:
		ORMapper (const std::string &dbName)
			: _dbName (dbName), _tblName (C::__ClassName),
			_fieldNames (_ExtractFieldName ())
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
				const C obj {};
				BOT_ORM_Impl::TypeVisitor visitor;
				obj.__Accept (visitor);

				auto strTypes = std::move (visitor.serializedTypes);
				size_t indexFieldName = 0;

				auto typeFmt = BOT_ORM_Impl::SplitStr (strTypes);
				auto strFmt = _fieldNames[indexFieldName++] + " " +
					std::move (typeFmt) + " primary key,";

				while (!strTypes.empty ())
				{
					typeFmt = BOT_ORM_Impl::SplitStr (strTypes);
					strFmt += _fieldNames[indexFieldName++] + " " +
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

				connector.Execute ("begin transaction;insert into " + _tblName +
								   " values (" + strIns + ");commit transaction;");
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

				// Only Set Key
				auto strDel = _fieldNames[0] + "=" +
					BOT_ORM_Impl::SplitStr (visitor.serializedValues);

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
				size_t indexFieldName = 0;

				std::string strKey;
				{
					auto val = BOT_ORM_Impl::SplitStr (strVals);
					strKey = _fieldNames[indexFieldName++] + "=" +
						std::move (val);
				}

				std::string strUpd;
				while (!strVals.empty ())
				{
					auto val = BOT_ORM_Impl::SplitStr (strVals);
					strUpd += _fieldNames[indexFieldName++] + "=" +
						std::move (val) + ",";
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
				std::string strUpdate ("begin transaction;");
				for (const auto &value : values)
				{
					BOT_ORM_Impl::ReaderVisitor visitor;
					value.__Accept (visitor);

					auto strVals = std::move (visitor.serializedValues);
					auto curIndex = 0;

					std::string strKey = _fieldNames[curIndex++] +
						"=" + BOT_ORM_Impl::SplitStr (strVals);

					std::string strUpd;
					while (!strVals.empty ())
						strUpd += _fieldNames[curIndex++] + "=" +
						BOT_ORM_Impl::SplitStr (strVals) + ",";
					strUpd.pop_back ();

					strUpdate += "update " + _tblName +
						" set " + std::move (strUpd) +
						" where " + std::move (strKey) + ";";
				}
				strUpdate += "commit transaction;";
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
					BOT_ORM_Impl::WriterVisitor visitor (serialized);
					obj.__Accept (visitor);
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
					ret = std::stol (argv[0]);
				});
			});
			if (isOk) return ret;
			else return -1;
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

				return _pMapper->_fieldNames[visitor.index];
			}
		};

		ORQuery Query (const C &queryHelper)
		{
			return ORQuery (queryHelper, this);
		}

	private:
		const std::string _dbName, _tblName;
		const std::vector<std::string> _fieldNames;
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

		static std::vector<std::string> _ExtractFieldName ()
		{
			std::string rawStr (C::__FieldNames), tmpStr;
			rawStr.push_back (',');
			tmpStr.reserve (rawStr.size ());

			std::vector<std::string> ret;
			for (const auto &ch : rawStr)
			{
				switch (ch)
				{
				case ',':
					ret.push_back (tmpStr);
					tmpStr.clear ();
					break;

				case '_':
				default:
					if (isalnum (ch) || isalpha (ch))
						tmpStr += ch;
					break;
				}
			}
			return std::move (ret);
		}
	};
}

#endif // !BOT_ORM_H