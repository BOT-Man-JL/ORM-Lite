
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
#include <type_traits>

#include "sqlite3.h"

// Public Macro

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

// Private Macro

#define __VISITOR                                         \
public:                                                   \
    template <typename... Args>                           \
    inline void Visit (Args & ... args)                   \
    {                                                     \
        return _Visit (args...);                          \
    }                                                     \
protected:                                                \
    template <typename T, typename... Args>               \
    inline void _Visit (T &property, Args & ... args)     \
    {                                                     \
        _Visit (property);                                \
        _Visit (args...);                                 \
    }                                                     \

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

	// Helper - Split String
	std::string SplitStr (std::string &inputStr,
						  char delim = char (0))
	{
		size_t pos = 0;
		if ((pos = inputStr.find (delim, 0)) != std::string::npos)
		{
			auto ret = inputStr.substr (0, pos);
			inputStr.erase (0, pos + 1);
			return std::move (ret);
		}
		return std::string ();
	}

	// Helper - Serialize
	template <typename T>
	std::string SerializeValue (T value)
	{
		std::stringstream strs;
		strs << value;
		return strs.str ();
	}

	template <>  // Assume not CV string
	inline std::string SerializeValue
		<std::string> (std::string value)
	{
		return "'" + value + "'";
	}

	// Helper - Deserialize
	template <typename T>
	inline void DeserializeValue (T &property, std::string value)
	{
		std::stringstream ostr (value);
		ostr >> property;
	}

	template <>  // Assume not CV string
	inline void DeserializeValue <std::string>
		(std::string &property, std::string value)
	{
		property = std::move (value);
	}

	class TypeVisitor
	{
		// Visitor Scheme
		__VISITOR

	public:
		std::string serializedTypes;
	protected:
		template <typename T>
		struct IsString
		{
			// Remarks:
			// Bad Support of std::remove_cv in gcc :-(
			constexpr static bool value =
				std::is_same<T, std::string>::value ||
				std::is_same<T, const std::string>::value ||
				std::is_same<T, volatile std::string>::value ||
				std::is_same<T, const volatile std::string>::value;
		};

		template <typename T>
		struct TypeString
		{
			// Remarks:
			// Visual Studio not support SFINAE :-(
			constexpr static const char *typeStr =
				std::is_integral<T>::value ? "integer" : (
					std::is_floating_point<T>::value ? "real" : (
						IsString<T>::value ? "text" : nullptr
						)
					);
			static_assert (typeStr != nullptr,
						   "Only Support Integral, Floating Point and"
						   " std::string :-(");
		};

		template <typename T>
		inline void _Visit (T &property)
		{
			serializedTypes += TypeString<T>::typeStr;
			serializedTypes += char (0);
		}
	};

	class ReaderVisitor
	{
		// Visitor Scheme
		__VISITOR

	private:
		char _delim;
	public:
		std::stringstream serializedValues;

		ReaderVisitor (char delim = char (0))
			: _delim (delim)
		{}
	protected:
		template <typename T>
		inline void _Visit (T &property)
		{
			serializedValues
				<< SerializeValue (T (property))
				<< _delim;
		}
	};

	class WriterVisitor
	{
		// Visitor Scheme
		__VISITOR

	private:
		std::string _serializedValues;
	public:
		WriterVisitor (std::string serializedValues)
			: _serializedValues (serializedValues)
		{}
	protected:
		template <typename T>
		inline void _Visit (T &property)
		{
			// Remarks:
			// GCC Hell: explicit specialization in non-namespace scope
			// So, unable to Write Impl Here
			DeserializeValue (property, SplitStr (_serializedValues));
		}
	};

	class IndexVisitor
	{
		// Visitor Scheme
		__VISITOR

	private:
		const void *_pointer;
	public:
		bool isFound;
		size_t index;

		IndexVisitor (const void *pointer)
			: index (0), isFound (false),
			_pointer (pointer)
		{}
	protected:
		template <typename T>
		inline void _Visit (const T &property)
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

		template <typename T>
		Expr (const T &property,
			  const std::string &relOp,
			  const T &value)
			: expr { std::make_pair (
				&property,
				relOp + BOT_ORM_Impl::SerializeValue (
					T (value))) }
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
			: _dbName (dbName), _tblName (C::__ClassName)
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
				BOT_ORM_Impl::TypeVisitor visitor;
				C ().__Accept (visitor);

				std::string strTypes = std::move (visitor.serializedTypes);
				size_t indexFieldName = 0;

				auto typeFmt = BOT_ORM_Impl::SplitStr (strTypes);
				auto strFmt = _FieldNames ()[indexFieldName++] + " " +
					std::move (typeFmt) + " primary key,";

				while (!strTypes.empty ())
				{
					typeFmt = BOT_ORM_Impl::SplitStr (strTypes);
					strFmt += _FieldNames ()[indexFieldName++] + " " +
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
				BOT_ORM_Impl::ReaderVisitor visitor (',');
				value.__Accept (visitor);

				std::string strIns (visitor.serializedValues.str ());
				strIns.pop_back ();

				connector.Execute ("begin transaction;insert into " + _tblName +
								   " values (" + std::move (strIns) +
								   ");commit transaction;");
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
					BOT_ORM_Impl::ReaderVisitor visitor (',');
					value.__Accept (visitor);

					std::string strValues (visitor.serializedValues.str ());
					strValues.pop_back ();

					strIns += "(" + std::move (strValues) + "),";
				}
				strIns.pop_back ();
				connector.Execute ("insert into " + _tblName +
								   " values " + std::move (strIns) + ";");
			});
		}

		bool Update (const C &value)
		{
			return _HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				BOT_ORM_Impl::ReaderVisitor visitor;
				value.__Accept (visitor);

				std::string strVals (visitor.serializedValues.str ());
				size_t indexFieldName = 0;

				std::string strKey;
				{
					auto val = BOT_ORM_Impl::SplitStr (strVals);
					strKey = _FieldNames ()[indexFieldName++] + "=" +
						std::move (val);
				}

				std::string strUpd;
				while (!strVals.empty ())
				{
					auto val = BOT_ORM_Impl::SplitStr (strVals);
					strUpd += _FieldNames ()[indexFieldName++] + "=" +
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

					std::string strVals (visitor.serializedValues.str ());
					auto curIndex = 0;

					std::string strKey = _FieldNames ()[curIndex++] +
						"=" + BOT_ORM_Impl::SplitStr (strVals);

					std::string strUpd;
					while (!strVals.empty ())
						strUpd += _FieldNames ()[curIndex++] + "=" +
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

		bool Delete (const C &value)
		{
			return _HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				BOT_ORM_Impl::ReaderVisitor visitor;
				value.__Accept (visitor);

				std::string strVals (visitor.serializedValues.str ());

				// Only Set Key
				auto strDel = _FieldNames ()[0] + "=" +
					BOT_ORM_Impl::SplitStr (strVals);

				connector.Execute ("delete from " + _tblName +
								   " where " + strDel + ";");
			});
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

			// Retrieve Select Result
			std::vector<C> ToVector ()
			{
				std::vector<C> ret;
				_pMapper->_Select (ret, _GetSQL ());
				return std::move (ret);
			}

			std::list<C> ToList ()
			{
				std::list<C> ret;
				_pMapper->_Select (ret, _GetSQL ());
				return std::move (ret);
			}

			// Count Result
			inline long Count ()
			{
				return _pMapper->_Count (_GetSQL ());
			}

			// Delete Values
			inline bool Delete ()
			{
				return _pMapper->_Delete (_GetSQL ());
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

				return _pMapper->_FieldNames ()[visitor.index];
			}

			std::string _GetSQL () const
			{
				if (!_sqlWhere.empty ())
					return " where (" + _sqlWhere + ")" + _sqlOrderBy + _sqlLimit;
				else
					return _sqlOrderBy + _sqlLimit;
			}
		};

		ORQuery Query (const C &queryHelper)
		{
			return ORQuery (queryHelper, this);
		}

	private:
		const std::string _dbName, _tblName;
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

		template <typename Out>
		bool _Select (Out &out,
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

		long _Count (const std::string &sqlStr = "")
		{
			long ret = 0;
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

		bool _Delete (const std::string &sqlStr)
		{
			return _HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				connector.Execute ("delete from " + _tblName +
								   " " + sqlStr + ";");
			});
		}

		static const std::vector<std::string> &_FieldNames ()
		{
			auto _ExtractFieldName = [] ()
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
			};

			static const std::vector<std::string> _fieldNames (
				_ExtractFieldName ());
			return _fieldNames;
		}
	};
}

#endif // !BOT_ORM_H