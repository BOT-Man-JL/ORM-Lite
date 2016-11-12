
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
#include <tuple>
#include <type_traits>

#include "sqlite3.h"

// Public Macro

#define ORMAP(_MY_CLASS_, ...)                            \
private:                                                  \
friend class BOT_ORM::ORMapper<_MY_CLASS_>;               \
inline decltype (auto) __ValTuple ()                      \
{ return std::forward_as_tuple (__VA_ARGS__); }           \
inline decltype (auto) __ValTuple () const                \
{ return std::forward_as_tuple (__VA_ARGS__); }           \
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

	// Helper - Serialize
	template <typename T>
	inline std::ostream &SerializeValue (std::ostream &os,
										 const T &value)
	{
		return os << value;
	}

	template <>  // Assume not CV string
	inline std::ostream &SerializeValue <std::string> (
		std::ostream &os, const std::string &value)
	{
		return os << "'" << value << "'";
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

	// Helper - Get TypeString
	template <typename T>
	const char *TypeString (T &)
	{
		// Remarks:
		// Visual Studio not support SFINAE :-(
		constexpr const char *typeStr =
			std::is_integral<T>::value ? "integer" : (
				std::is_floating_point<T>::value ? "real" : (
					std::is_same<typename std::remove_cv<T>::type,
					std::string>::value ? "text" : nullptr
					)
				);

		static_assert (typeStr != nullptr,
					   "Only Support Integral, Floating Point and"
					   " std::string :-(");

		return typeStr;
	}

	// Helper - Tuple Visitor
	// Refrence:
	// http://stackoverflow.com/questions/18155533

	// Using a _SizeT to specify the Index :-), Cool
	template < size_t > struct _SizeT {};

	template < typename TupleType, typename ActionType >
	inline void TupleVisitor (TupleType &tuple, ActionType action)
	{
		TupleVisitor_Impl (tuple, action,
						   _SizeT<std::tuple_size<TupleType>::value> ());
	}

	template < typename TupleType, typename ActionType >
	inline void TupleVisitor_Impl (TupleType &tuple, ActionType action,
								   _SizeT<0>) {}

	template < typename TupleType, typename ActionType, size_t N >
	inline void TupleVisitor_Impl (TupleType &tuple, ActionType action,
								   _SizeT<N>)
	{
		TupleVisitor_Impl (tuple, action, _SizeT<N - 1> ());
		action (std::get<N - 1> (tuple));
	}
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
				&property, relOp) }
		{
			std::ostringstream os;
			BOT_ORM_Impl::SerializeValue (os, value);
			expr.front ().second.append (os.str ());
		}

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
		{
			static_assert (std::is_copy_constructible<C>::value,
						   "It must be Copy Constructible");
		}

		inline const std::string &ErrMsg () const
		{
			return _errMsg;
		}

		typename std::enable_if<std::is_default_constructible<C>::value,
			bool>::type CreateTbl ()
		{
			C value {};
			return CreateTbl (value);
		}

		bool CreateTbl (const C &value)
		{
			return _HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				const auto &fieldNames = ORMapper<C>::_FieldNames ();
				size_t index = 0;
				std::vector<std::string> strTypes (fieldNames.size ());
				auto tuple = value.__ValTuple ();
				BOT_ORM_Impl::TupleVisitor (
					tuple, [&strTypes, &index] (auto &val)
				{
					strTypes[index++] = BOT_ORM_Impl::TypeString (val);
				});

				auto strFmt = fieldNames[0] + " " + strTypes[0] +
					" primary key,";
				for (size_t i = 1; i < fieldNames.size (); i++)
					strFmt += fieldNames[i] + " " + strTypes[i] + ",";
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
				std::ostringstream os;
				size_t index = 0;
				auto tuple = value.__ValTuple ();
				BOT_ORM_Impl::TupleVisitor (
					tuple, [&os, &index] (auto &val)
				{
					if (++index != ORMapper<C>::_FieldNames ().size ())
						BOT_ORM_Impl::SerializeValue (os, val) << ',';
					else
						BOT_ORM_Impl::SerializeValue (os, val);
				});

				connector.Execute ("insert into " + _tblName +
								   " values (" + os.str () + ");");
			});
		}

		template <typename In>
		bool Insert (const In &values)
		{
			return _HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				std::ostringstream os;
				size_t count = 0;
				for (const auto &value : values)
				{
					os << "(";

					size_t index = 0;
					auto tuple = value.__ValTuple ();
					BOT_ORM_Impl::TupleVisitor (
						tuple, [&os, &index] (auto &val)
					{
						if (++index != ORMapper<C>::_FieldNames ().size ())
							BOT_ORM_Impl::SerializeValue (os, val) << ',';
						else
							BOT_ORM_Impl::SerializeValue (os, val);
					});

					if (++count != values.size ())
						os << "),";
					else
						os << ")";
				}

				connector.Execute ("insert into " + _tblName +
								   " values " + os.str () + ";");
			});
		}

		bool Update (const C &value)
		{
			return _HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				const auto &fieldNames = ORMapper<C>::_FieldNames ();

				size_t index = 0;
				std::stringstream os, osKey;
				auto tuple = value.__ValTuple ();
				BOT_ORM_Impl::TupleVisitor (
					tuple, [&os, &osKey, &index, &fieldNames] (auto &val)
				{
					if (index == 0)
					{
						osKey << fieldNames[index++] << "=";
						BOT_ORM_Impl::SerializeValue (osKey, val);
					}
					else
					{
						os << fieldNames[index++] << "=";
						if (index != fieldNames.size ())
							BOT_ORM_Impl::SerializeValue (os, val) << ',';
						else
							BOT_ORM_Impl::SerializeValue (os, val);
					}
				});

				connector.Execute ("update " + _tblName +
								   " set " + os.str () +
								   " where " + osKey.str () + ";");
			});
		}

		template <typename In>
		bool Update (const In &values)
		{
			return _HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				const auto &fieldNames = ORMapper<C>::_FieldNames ();

				std::stringstream os;
				os << "begin transaction;";
				for (const auto &value : values)
				{
					os << "update " << _tblName << " set ";

					size_t index = 0;
					std::stringstream osKey;
					auto tuple = value.__ValTuple ();
					BOT_ORM_Impl::TupleVisitor (
						tuple, [&os, &osKey, &index, &fieldNames] (auto &val)
					{
						if (index == 0)
						{
							osKey << fieldNames[index++] << "=";
							BOT_ORM_Impl::SerializeValue (osKey, val);
						}
						else
						{
							os << fieldNames[index++] << "=";
							if (index != fieldNames.size ())
								BOT_ORM_Impl::SerializeValue (os, val) << ',';
							else
								BOT_ORM_Impl::SerializeValue (os, val);
						}
					});

					os << " where " + osKey.str () + ";";
				}
				os << "commit transaction;";
				connector.Execute (os.str ());
			});
		}

		bool Delete (const C &value)
		{
			return _HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				std::stringstream os;
				size_t index = 0;
				auto tuple = value.__ValTuple ();
				BOT_ORM_Impl::TupleVisitor (
					tuple, [&os, &index] (auto &val)
				{
					// Only Set Key
					if (index == 0)
					{
						os << ORMapper<C>::_FieldNames ()[0] << "=";
						BOT_ORM_Impl::SerializeValue (os, val);
						index++;
					}
				});

				connector.Execute ("delete from " + _tblName +
								   " where " + os.str () + ";");
			});
		}

		class ORQuery
		{
		public:
			ORQuery (C &queryHelper, ORMapper<C> *pMapper)
				: _queryHelper (queryHelper), _pMapper (pMapper)
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
				_pMapper->_Select (_queryHelper, ret, _GetSQL ());
				return std::move (ret);
			}

			std::list<C> ToList ()
			{
				std::list<C> ret;
				_pMapper->_Select (_queryHelper, ret, _GetSQL ());
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
			C &_queryHelper;
			ORMapper<C> *_pMapper;
			std::string _sqlWhere;
			std::string _sqlOrderBy, _sqlLimit;

			std::string _GetFieldName (const void *property)
			{
				bool isFound = false;
				size_t index = 0;
				auto tuple = _queryHelper.__ValTuple ();
				BOT_ORM_Impl::TupleVisitor (
					tuple, [&property, &isFound, &index] (auto &val)
				{
					if (!isFound && property == &val)
						isFound = true;
					else if (!isFound)
						index++;
				});

				if (!isFound)
					throw std::runtime_error ("No such Field in the Table");
				return _pMapper->ORMapper<C>::_FieldNames ()[index];
			}

			std::string _GetSQL () const
			{
				if (!_sqlWhere.empty ())
					return " where (" + _sqlWhere + ")" +
					_sqlOrderBy + _sqlLimit;
				else
					return _sqlOrderBy + _sqlLimit;
			}
		};

		ORQuery Query (C &queryHelper)
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
		bool _Select (C &queryHelper, Out &out,
					  const std::string &sqlStr = "")
		{
			return _HandleException ([&] (
				BOT_ORM_Impl::SQLConnector &connector)
			{
				connector.Execute ("select * from " + _tblName +
								   " " + sqlStr + ";",
								   [&] (int argc, char **argv, char **)
				{
					if (argc != ORMapper<C>::_FieldNames ().size ())
						throw std::runtime_error (
							"Inter ORM Error: Select argc != Field Count");

					size_t index = 0;
					auto tuple = queryHelper.__ValTuple ();
					BOT_ORM_Impl::TupleVisitor (
						tuple, [&argv, &index] (auto &val)
					{
						BOT_ORM_Impl::DeserializeValue (val, argv[index++]);
					});
					out.push_back (queryHelper);
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