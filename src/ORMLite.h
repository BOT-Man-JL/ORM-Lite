
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
#include <tuple>
#include <unordered_map>
#include <cctype>
#include <thread>
#include <sstream>
#include <type_traits>
#include <cstddef>

#include "sqlite3.h"

// Public Macro

#define ORMAP(_TABLE_NAME_, ...)                          \
private:                                                  \
friend class BOT_ORM::ORMapper;                           \
template <typename FN>                                    \
void __Accept (FN fn)                                     \
{                                                         \
    BOT_ORM_Impl::FnVisitor::Visit (fn, __VA_ARGS__);     \
}                                                         \
template <typename FN>                                    \
void __Accept (FN fn) const                               \
{                                                         \
    BOT_ORM_Impl::FnVisitor::Visit (fn, __VA_ARGS__);     \
}                                                         \
auto __Tuple () const                                     \
{                                                         \
    return std::make_tuple (__VA_ARGS__);                 \
}                                                         \
static const std::vector<std::string> &__FieldNames ()    \
{                                                         \
    static const std::vector<std::string> _fieldNames {   \
        BOT_ORM_Impl::ExtractFieldName (#__VA_ARGS__) };  \
    return _fieldNames;                                   \
}                                                         \
constexpr static const char *__TableName =  _TABLE_NAME_; \

// Nullable Template
// http://stackoverflow.com/questions/2537942/nullable-values-in-c/28811646#28811646

namespace BOT_ORM
{
	template <typename T>
	class Nullable
	{
		template <typename T2>
		friend bool operator== (const Nullable<T2> &op1,
								const Nullable<T2> &op2);
		template <typename T2>
		friend bool operator== (const Nullable<T2> &op,
								const T2 &value);
		template <typename T2>
		friend bool operator== (const T2 &value,
								const Nullable<T2> &op);
		template <typename T2>
		friend bool operator== (const Nullable<T2> &op,
								nullptr_t);
		template <typename T2>
		friend bool operator== (nullptr_t,
								const Nullable<T2> &op);
	public:
		// Default or Null Construction
		Nullable ()
			: m_hasValue (false), m_value (T ()) {}
		Nullable (nullptr_t)
			: Nullable () {}

		// Null Assignment
		const Nullable<T> & operator= (nullptr_t)
		{
			m_hasValue = false;
			m_value = T ();
			return *this;
		}

		// Value Construction
		Nullable (const T &value)
			: m_hasValue (true), m_value (value) {}

		// Value Assignment
		const Nullable<T> & operator= (const T &value)
		{
			m_hasValue = true;
			m_value = value;
			return *this;
		}

	private:
		bool m_hasValue;
		T m_value;

	public:
		const T &Value () const
		{ return m_value; }
	};

	template <typename T2>
	inline bool operator== (const Nullable<T2> &op1,
							const Nullable<T2> &op2)
	{
		return op1.m_hasValue == op2.m_hasValue &&
			(!op1.m_hasValue || op1.m_value == op2.m_value);
	}

	template <typename T2>
	inline bool operator== (const Nullable<T2> &op,
							const T2 &value)
	{
		return op.m_hasValue && op.m_value == value;
	}

	template <typename T2>
	inline bool operator== (const T2 &value,
							const Nullable<T2> &op)
	{
		return op.m_hasValue && op.m_value == value;
	}

	template <typename T2>
	inline bool operator== (const Nullable<T2> &op,
							nullptr_t)
	{
		return !op.m_hasValue;
	}

	template <typename T2>
	inline bool operator== (nullptr_t,
							const Nullable<T2> &op)
	{
		return !op.m_hasValue;
	}
}

// Helpers

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
	inline std::ostream &SerializeValue (
		std::ostream &os,
		const T &value)
	{
		return os << value;
	}

	template <>
	inline std::ostream &SerializeValue <std::string> (
		std::ostream &os,
		const std::string &value)
	{
		return os << "'" << value << "'";
	}

	template <typename T>
	inline std::ostream &SerializeValue (
		std::ostream &os,
		const BOT_ORM::Nullable<T> &value)
	{
		if (value == nullptr)
			return os << "null";
		return SerializeValue (os, value.Value ());
	}

	// Helper - Deserialize

	template <typename T>
	inline void DeserializeValue (
		T &property, const char *value)
	{
		std::stringstream ostr (value);
		ostr >> property;
	}

	template <>
	inline void DeserializeValue <std::string> (
		std::string &property, const char *value)
	{
		property = value;
	}

	template <typename T>
	inline void DeserializeValue (
		BOT_ORM::Nullable<T> &property, const char *value)
	{
		if (value)
		{
			T val;
			DeserializeValue (val, value);
			property = val;
		}
		else
			property = nullptr;
	}

	// Helper - Get TypeString

	template <typename T>
	const char *TypeString (const T &)
	{
		constexpr static const char *typeStr =
			(std::is_integral<T>::value &&
			 !std::is_same<std::remove_cv_t<T>, char>::value &&
			 !std::is_same<std::remove_cv_t<T>, wchar_t>::value &&
			 !std::is_same<std::remove_cv_t<T>, char16_t>::value &&
			 !std::is_same<std::remove_cv_t<T>, char32_t>::value &&
			 !std::is_same<std::remove_cv_t<T>, unsigned char>::value)
			? "integer not null"
			: (std::is_floating_point<T>::value)
			? "real not null"
			: (std::is_same<std::remove_cv_t<T>, std::string>::value)
			? "text not null"
			: nullptr;

		static_assert (
			typeStr != nullptr,
			"Only Support Integral, Floating Point and std::string :-)");

		return typeStr;;
	}

	template <typename T>
	const char *TypeString (const BOT_ORM::Nullable<T> &)
	{
		constexpr static const char *typeStr =
			(std::is_integral<T>::value &&
			 !std::is_same<std::remove_cv_t<T>, char>::value &&
			 !std::is_same<std::remove_cv_t<T>, wchar_t>::value &&
			 !std::is_same<std::remove_cv_t<T>, char16_t>::value &&
			 !std::is_same<std::remove_cv_t<T>, char32_t>::value &&
			 !std::is_same<std::remove_cv_t<T>, unsigned char>::value)
			? "integer"
			: (std::is_floating_point<T>::value)
			? "real"
			: (std::is_same<std::remove_cv_t<T>, std::string>::value)
			? "text"
			: nullptr;

		static_assert (
			typeStr != nullptr,
			"Only Support Integral, Floating Point and std::string :-)");

		return typeStr;;
	}

	// Injection Helper - Fn Visitor
	class FnVisitor
	{
	public:
		template <typename Fn, typename... Args>
		static inline void Visit (Fn fn, Args & ... args)
		{
			_Visit (fn, args...);
		}

	protected:
		template <typename Fn, typename T, typename... Args>
		static inline void _Visit (Fn fn, T &property, Args & ... args)
		{
			if (_Visit (fn, property))
				_Visit (fn, args...);
		}

		template <typename Fn, typename T>
		static inline bool _Visit (Fn fn, T &property)
		{
			return fn (property);
		}
	};

	// Injection Helper - Extract Field Names
	std::vector<std::string> ExtractFieldName (std::string input)
	{
		std::vector<std::string> ret;
		std::string tmpStr;

		for (const auto &ch : std::move (input) + ",")
		{
			if (isalnum (ch) || ch == '_')
				tmpStr += ch;
			else if (ch == ',')
			{
				ret.push_back (tmpStr);
				tmpStr.clear ();
			}
		}
		return std::move (ret);
	};
}

namespace BOT_ORM
{
	namespace Expression
	{
		// SetExpr

		struct SetExpr
		{
			SetExpr (std::string field_op_val)
				: expr { std::move (field_op_val) }
			{}

			const std::string &ToString () const
			{ return expr; }

		protected:
			std::string expr;
		};

		// Comparable

		template <typename T>
		struct Comparable
		{
			std::string fieldName;
			const char *prefixStr;
		};

		// Field : Comparable

		template <typename T>
		struct Field : public Comparable<T>
		{
			Field (std::string fieldName,
				   const char *tableName)
				: Comparable<T> { std::move (fieldName), tableName } {}

			inline SetExpr operator = (T value)
			{
				std::ostringstream os;
				BOT_ORM_Impl::SerializeValue (
					os << this->fieldName << "=", value);
				return SetExpr { os.str () };
			}
		};

		// Nullable Field : Field : Comparable

		template <typename T>
		struct NullableField : public Field<T>
		{
			NullableField (std::string fieldName,
						   const char *tableName)
				: Field<T> { std::move (fieldName), tableName } {}

			inline SetExpr operator = (T value)
			{
				std::ostringstream os;
				BOT_ORM_Impl::SerializeValue (
					os << this->fieldName << "=", value);
				return SetExpr { os.str () };
			}

			inline SetExpr operator = (nullptr_t)
			{ return SetExpr { this->fieldName + "=null" }; }
		};

		// Aggregate Function : Comparable

		template <typename T>
		struct AggregateFunc : public Comparable<T>
		{
			AggregateFunc (std::string function)
				: Comparable<T> { std::move (function), nullptr } {}

			AggregateFunc (std::string function, const Field<T> &field)
				: Comparable<T> { function + "(" + field.prefixStr +
				"." + field.fieldName + ")" } {}
		};

		// Expr

		class Expr
		{
		public:
			template <typename T>
			Expr (const Comparable<T> &field,
				  std::string op_val)
				: exprs { { field.fieldName + op_val, field.prefixStr } }
			{}

			template <typename T>
			Expr (const Comparable<T> &field,
				  std::string op, T value)
			{
				std::ostringstream os;
				BOT_ORM_Impl::SerializeValue (
					os << field.fieldName << op, value);
				exprs.emplace_back (os.str (), field.prefixStr);
			}

			template <typename T>
			Expr (const Field<T> &field1,
				  std::string op,
				  const Field<T> &field2)
				: exprs
			{
				{ field1.fieldName, field1.prefixStr },
				{ std::move (op), nullptr },
				{ field2.fieldName, field2.prefixStr }
			}
			{}

			std::string ToString (bool withField) const
			{
				std::stringstream out;
				for (const auto &expr : exprs)
				{
					if (withField && expr.second != nullptr)
						out << expr.second << ".";
					out << expr.first;
				}
				return out.str ();
			}

			inline Expr operator && (const Expr &right) const
			{ return And_Or (right, " and "); }
			inline Expr operator || (const Expr &right) const
			{ return And_Or (right, " or "); }

		protected:
			std::list<std::pair<std::string, const char*>> exprs;

			Expr And_Or (const Expr &right, std::string logOp) const
			{
				auto ret = *this;
				auto rigthExprs = right.exprs;
				ret.exprs.emplace_front ("(", nullptr);
				ret.exprs.emplace_back (std::move (logOp), nullptr);
				ret.exprs.splice (ret.exprs.cend (),
								  std::move (rigthExprs));
				ret.exprs.emplace_back (")", nullptr);
				return std::move (ret);
			}
		};

		// Field ? Field

		template <typename T>
		inline Expr operator == (const Field<T> &op1, const Field<T> &op2)
		{ return Expr { op1 , "=", op2 }; }

		template <typename T>
		inline Expr operator != (const Field<T> &op1, const Field<T> &op2)
		{ return Expr { op1, "!=", op2 }; }

		template <typename T>
		inline Expr operator > (const Field<T> &op1, const Field<T> &op2)
		{ return Expr { op1 , ">", op2 }; }

		template <typename T>
		inline Expr operator >= (const Field<T> &op1, const Field<T> &op2)
		{ return Expr { op1, ">=", op2 }; }

		template <typename T>
		inline Expr operator < (const Field<T> &op1, const Field<T> &op2)
		{ return Expr { op1 , "<", op2 }; }

		template <typename T>
		inline Expr operator <= (const Field<T> &op1, const Field<T> &op2)
		{ return Expr { op1, "<=", op2 }; }

		// Field ? Value

		template <typename T>
		inline Expr operator == (const Comparable<T> &op, T value)
		{ return Expr (op, "=", std::move (value)); }

		template <typename T>
		inline Expr operator != (const Comparable<T> &op, T value)
		{ return Expr (op, "!=", std::move (value)); }

		template <typename T>
		inline Expr operator > (const Comparable<T> &op, T value)
		{ return Expr (op, ">", std::move (value)); }

		template <typename T>
		inline Expr operator >= (const Comparable<T> &op, T value)
		{ return Expr (op, ">=", std::move (value)); }

		template <typename T>
		inline Expr operator < (const Comparable<T> &op, T value)
		{ return Expr (op, "<", std::move (value)); }

		template <typename T>
		inline Expr operator <= (const Comparable<T> &op, T value)
		{ return Expr (op, "<=", std::move (value)); }

		template <typename T>
		inline Expr operator == (const NullableField<T> &op, nullptr_t)
		{ return Expr { op, " is null" }; }

		template <typename T>
		inline Expr operator != (const NullableField<T> &op, nullptr_t)
		{ return Expr { op, " is not null" }; }

		// StringField &/| String

		inline Expr operator & (const Field<std::string> &field,
								std::string val)
		{ return Expr (field, " like ", std::move (val)); }

		inline Expr operator | (const Field<std::string> &field,
								std::string val)
		{ return Expr (field, " not like ", std::move (val)); }
	}

	// ORMapper

	class ORMapper
	{
	public:
		ORMapper (const std::string &connectionString)
			: _connector (connectionString) {}

		template <typename Fn>
		void Transaction (Fn fn)
		{
			try
			{
				_connector.Execute ("begin transaction;");
				fn ();
				_connector.Execute ("commit transaction;");
			}
			catch (...)
			{
				_connector.Execute ("rollback transaction;");
				throw;
			}
		}

		template <typename C>
		void CreateTbl (const C &entity)
		{
			const auto &fieldNames = C::__FieldNames ();
			std::vector<std::string> strTypes (fieldNames.size ());
			size_t index = 0;

			entity.__Accept ([&strTypes, &index] (auto &val)
			{
				strTypes[index++] = BOT_ORM_Impl::TypeString (val);
				return true;
			});

			auto strFmt = fieldNames[0] + " " + strTypes[0] +
				" primary key,";
			for (size_t i = 1; i < fieldNames.size (); i++)
				strFmt += fieldNames[i] + " " + strTypes[i] + ",";
			strFmt.pop_back ();

			_connector.Execute ("create table " +
								std::string (C::__TableName) +
								"(" + strFmt + ");");
		}

		template <typename C>
		void DropTbl (const C &)
		{
			_connector.Execute ("drop table " +
								std::string (C::__TableName) +
								";");
		}

		template <typename C>
		void Insert (const C &entity, bool withId = true)
		{
			std::ostringstream os;
			size_t index = 0;
			auto fieldCount = C::__FieldNames ().size ();

			entity.__Accept ([&os, &index, fieldCount, withId] (auto &val)
			{
				if (index == 0 && !withId)
					os << "null";
				else
					BOT_ORM_Impl::SerializeValue (os, val);
				if (++index != fieldCount)
					os << ',';
				return true;
			});

			_connector.Execute ("insert into " +
								std::string (C::__TableName) +
								" values (" + os.str () + ");");
		}

		template <typename In>
		void InsertRange (const In &entities, bool withId = true)
		{
			using C = typename In::value_type;

			std::ostringstream os;
			size_t count = 0;
			auto fieldCount = C::__FieldNames ().size ();

			for (const auto &entity : entities)
			{
				os << "(";

				size_t index = 0;
				entity.__Accept (
					[&os, &index, fieldCount, withId] (auto &val)
				{
					if (index == 0 && !withId)
						os << "null";
					else
						BOT_ORM_Impl::SerializeValue (os, val);
					if (++index != fieldCount)
						os << ',';
					return true;
				});

				if (++count != entities.size ())
					os << "),";
				else
					os << ")";
			}

			_connector.Execute ("insert into " +
								std::string (C::__TableName) +
								" values " + os.str () + ";");
		}

		template <typename C>
		void Update (const C &entity)
		{
			std::stringstream os, osKey;
			size_t index = 0;
			const auto &fieldNames = C::__FieldNames ();

			os << "set ";
			osKey << "where ";

			entity.__Accept (
				[&os, &osKey, &index, &fieldNames] (auto &val)
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
				return true;
			});

			_connector.Execute ("update " +
								std::string (C::__TableName) +
								" " + os.str () +
								" " + osKey.str () + ";");
		}

		template <typename In>
		void UpdateRange (const In &entities)
		{
			using C = typename In::value_type;

			std::stringstream os;
			const auto &fieldNames = C::__FieldNames ();

			for (const auto &entity : entities)
			{
				os << "update "
					<< C::__TableName
					<< " set ";

				size_t index = 0;
				std::stringstream osKey;

				entity.__Accept (
					[&os, &osKey, &index, &fieldNames] (auto &val)
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
							BOT_ORM_Impl::SerializeValue (os, val)
							<< ',';
						else
							BOT_ORM_Impl::SerializeValue (os, val);
					}
					return true;
				});
				os << " where " + osKey.str () + ";";
			}
			_connector.Execute (os.str ());
		}

		template <typename C, typename... Args>
		void Update (const C &,
					 const Expr &expr,
					 const Args & ... setExprs)
		{
			std::stringstream setSql;
			setSql << "set ";
			auto count = sizeof... (Args);
			BOT_ORM_Impl::FnVisitor::Visit (
				[&setSql, &count] (const SetExpr &setExpr)
			{
				setSql << setExpr.ToString ();
				if (--count != 0) setSql << ",";
				return true;
			}, setExprs...);
			_connector.Execute ("update " +
								std::string (C::__TableName) +
								" " + setSql.str () +
								" where " + expr.ToString (false) + ";");
		}

		template <typename C>
		void Delete (const C &entity)
		{
			std::stringstream os;
			os << "where ";

			entity.__Accept ([&os] (auto &val)
			{
				os << C::__FieldNames ()[0] << "=";
				BOT_ORM_Impl::SerializeValue (os, val);
				return false;
			});

			_connector.Execute ("delete from " +
								std::string (C::__TableName) +
								" " + os.str () + ";");
		}

		template <typename C>
		inline void Delete (const C &,
							const Expr &expr)
		{
			_connector.Execute ("delete from " +
								std::string (C::__TableName) +
								" where " + expr.ToString (false) + ";");
		}

		// Query Object :-)

		template <typename QueryResult>
		class Queryable
		{
		public:
			Queryable (ORMapper *pMapper,
					   QueryResult queryHelper,
					   std::string queryFrom,
					   std::string queryTarget = "*",
					   std::string sqlWhere = "",
					   std::string sqlGroupBy = "",
					   std::string sqlHaving = "",
					   std::string sqlOrderBy = "",
					   std::string sqlLimit = "",
					   std::string sqlOffset = "") :
				_pMapper (pMapper),
				_queryHelper (std::move (queryHelper)),
				_sqlFrom (std::move (queryFrom)),
				_sqlTarget (std::move (queryTarget)),
				_sqlWhere (std::move (sqlWhere)),
				_sqlGroupBy (std::move (sqlGroupBy)),
				_sqlHaving (std::move (sqlHaving)),
				_sqlOrderBy (std::move (sqlOrderBy)),
				_sqlLimit (std::move (sqlLimit)),
				_sqlOffset (std::move (sqlOffset))
			{}

			// Result Type
			typedef QueryResult result_type;

			// Where
			inline Queryable &Where (const Expr &expr)
			{
				_sqlWhere = " where (" + expr.ToString (true) + ")";
				return *this;
			}

			// Group By
			template <typename T>
			inline Queryable &GroupBy (const Field<T> &field)
			{
				_sqlGroupBy = " group by " + _GetFieldSql (field);
				return *this;
			}
			inline Queryable &Having (const Expr &expr)
			{
				_sqlHaving = " having " + expr.ToString (true);
				return *this;
			}

			// Order By
			template <typename T>
			inline Queryable &OrderBy (const Field<T> &field)
			{
				if (_sqlOrderBy.empty ())
					_sqlOrderBy = " order by " + _GetFieldSql (field);
				else
					_sqlOrderBy += "," + _GetFieldSql (field);
				return *this;
			}
			template <typename T>
			inline Queryable &OrderByDescending (const Field<T> &field)
			{
				if (_sqlOrderBy.empty ())
					_sqlOrderBy = " order by " + _GetFieldSql (field) + " desc";
				else
					_sqlOrderBy += "," + _GetFieldSql (field) + " desc";
				return *this;
			}

			// Limit and Offset
			inline Queryable &Take (size_t count)
			{
				_sqlLimit = " limit " + std::to_string (count);
				return *this;
			}
			inline Queryable &Skip (size_t count)
			{
				// ~0 is a trick :-)
				if (_sqlLimit.empty ()) _sqlLimit = " limit ~0";
				_sqlOffset = " offset " + std::to_string (count);
				return *this;
			}

			// Join
			template <typename C>
			inline auto Join (const C &queryHelper2,
							  const Expr &onExpr) const
			{
				return _NewQuery (
					_sqlTarget,
					_sqlFrom + " join " +
					std::string (C::__TableName) +
					" on " + onExpr.ToString (true),
					_ToTuple (_queryHelper, queryHelper2));
			}
			template <typename C>
			inline auto LeftJoin (const C &queryHelper2,
								  const Expr &onExpr) const
			{
				return _NewQuery (
					_sqlTarget,
					_sqlFrom + " left join " +
					std::string (C::__TableName) +
					" on " + onExpr.ToString (true),
					_ToTuple (_queryHelper, queryHelper2));
			}

			// Select
			template <typename... Args>
			inline auto Select (const Args & ... args) const
			{
				return _NewQuery (
					_GetFieldSql (args...),
					_sqlFrom,
					_SelectToTuple (args...)
				);
			}

			// Aggregate
			template <typename T>
			Nullable<T> Aggregate (const AggregateFunc<T> &agg) const
			{
				Nullable<T> ret;
				_pMapper->_Select (
					agg.fieldName, _GetFromSql (),
					[&] (int argc, char **argv, char **)
				{
					BOT_ORM_Impl::DeserializeValue (ret, argv[0]);
				});
				return std::move (ret);
			}

			// Retrieve Select Result
			std::vector<QueryResult> ToVector () const
			{
				std::vector<QueryResult> ret;
				_Select (_queryHelper, ret);
				return std::move (ret);
			}
			std::list<QueryResult> ToList () const
			{
				std::list<QueryResult> ret;
				_Select (_queryHelper, ret);
				return std::move (ret);
			}

		protected:
			ORMapper *_pMapper;
			QueryResult _queryHelper;

			std::string _sqlFrom;
			std::string _sqlTarget;

			std::string _sqlWhere;
			std::string _sqlGroupBy;
			std::string _sqlHaving;
			std::string _sqlOrderBy;
			std::string _sqlLimit, _sqlOffset;

			// Return "from ..." for Query
			inline std::string _GetFromSql () const
			{
				return _sqlFrom +
					_sqlWhere +
					_sqlGroupBy + _sqlHaving +
					_sqlOrderBy +
					_sqlLimit + _sqlOffset;
			}

			// Return a new Queryable Object
			template <typename... Args>
			auto _NewQuery (std::string target,
							std::string from,
							std::tuple<Args...> newQueryHelper) const
			{
				return Queryable<std::tuple<Args...>>
				{
					_pMapper, std::move (newQueryHelper),
						std::move (from), std::move (target),
						_sqlWhere,
						_sqlGroupBy, _sqlHaving,
						_sqlOrderBy,
						_sqlLimit, _sqlOffset
				};
			}

			// Select for Normal Objects
			template <typename C, typename Out>
			void _Select (const C &, Out &out) const
			{
				auto copy = _queryHelper;
				_pMapper->_Select (_sqlTarget, _GetFromSql (),
								   [&] (int, char **argv, char **)
				{
					size_t index = 0;
					copy.__Accept ([&argv, &index] (auto &val)
					{
						BOT_ORM_Impl::DeserializeValue (val, argv[index++]);
						return true;
					});
					out.push_back (copy);
				});
			}

			// Select for Tuples
			template <typename Out, typename... Args>
			void _Select (const std::tuple<Args...> &, Out &out) const
			{
				auto copy = _queryHelper;
				_pMapper->_Select (_sqlTarget, _GetFromSql (),
								   [&] (int, char **argv, char **)
				{
					size_t index = 0;
					_TupleHelper<QueryResult, sizeof... (Args)>::Visit (
						copy, [&argv, &index] (auto &val)
					{
						BOT_ORM_Impl::DeserializeValue (val, argv[index++]);
						return true;
					});
					out.push_back (copy);
				});
			}

			// Static Helper Functions

			// Return Field Strings for OrderBy and Select
			template <typename T>
			static inline std::string _GetFieldSql (
				const Comparable<T> &op)
			{
				if (op.prefixStr)
					return op.prefixStr + ("." + op.fieldName);
				else
					return op.fieldName;
			}

			template <typename T, typename... Args>
			static inline std::string _GetFieldSql (const Comparable<T> &arg,
													const Args & ... args)
			{
				return _GetFieldSql (arg) + "," + _GetFieldSql (args...);
			}

			// Return Select Target Tuple
			template <typename T>
			static inline auto _SelectToTuple (const Comparable<T> &)
			{ return std::make_tuple (Nullable<T> {}); }

			template <typename T, typename... Args>
			static inline auto _SelectToTuple (const Comparable<T> &arg,
											   const Args & ... args)
			{
				return std::tuple_cat (_SelectToTuple (arg),
									   _SelectToTuple (args...));
			}

			// To Nullable
			// Get Nullable Value Wrappers for non-nullable Types
			template <typename T>
			static inline auto _ToNullable (const T &val)
			{
				return Nullable<T> (val);
			}
			template <typename T>
			static inline auto _ToNullable (const Nullable<T> &val)
			{
				return val;
			}

			// Tuple Nullable-Cat
			// Covert Tuple to Tuple with all Nullable Fields
			template <typename TupleType, size_t N>
			struct _TupleHelper
			{
				static inline auto ToNullable (const TupleType &tuple)
				{
					return std::tuple_cat (
						_TupleHelper<TupleType, N - 1>::ToNullable (tuple),
						std::make_tuple (_ToNullable (std::get<N - 1> (tuple)))
					);
				}

				template <typename Fn>
				static inline void Visit (TupleType &tuple, Fn fn)
				{
					_TupleHelper<TupleType, N - 1>::Visit (tuple, fn);
					fn (std::get<N - 1> (tuple));
				}
			};
			template <typename TupleType>
			struct _TupleHelper <TupleType, 1>
			{
				static inline auto ToNullable (const TupleType &tuple)
				{
					return std::make_tuple (
						_ToNullable (std::get<0> (tuple)));
				}

				template <typename Fn>
				static inline void Visit (TupleType &tuple, Fn fn)
				{
					fn (std::get<0> (tuple));
				}
			};

			// Flaten Arguments into Tuple
			template <typename C>
			static inline auto _ToTuple (const C &arg)
			{
				using TupleType = decltype (arg.__Tuple ());
				constexpr size_t size = std::tuple_size<TupleType>::value;
				return _TupleHelper<TupleType, size>::ToNullable (
					arg.__Tuple ());
			}
			template <typename... Args>
			static inline auto _ToTuple (const std::tuple<Args...>& t)
			{
				// _TupleHelper is not necessary
				return t;
			}
			template <typename Arg, typename... Args>
			static inline auto _ToTuple (const Arg &arg,
										 const Args & ... args)
			{
				return std::tuple_cat (_ToTuple (arg),
									   _ToTuple (args...));
			}
		};

		template <typename C>
		inline Queryable<C> Query (C queryHelper)
		{
			static_assert (std::is_copy_constructible<C>::value,
						   "It must be Copy Constructible");

			return Queryable<C> (this, std::move (queryHelper),
								 std::string (C::__TableName));
		}

		class FieldExtractor
		{
			using pair_type = std::pair<std::string, const char *>;

		public:
			template <typename... Args>
			FieldExtractor (const Args & ... args)
			{
				BOT_ORM_Impl::FnVisitor::Visit ([this] (auto &helper)
				{
					const auto &fieldNames =
						std::remove_reference_t<
						std::remove_cv_t<decltype (helper)>
						>::__FieldNames ();
					constexpr auto tableName =
						std::remove_reference_t<
						std::remove_cv_t<decltype (helper)>
						>::__TableName;

					size_t index = 0;
					helper.__Accept (
						[this, &index, &fieldNames, &tableName] (auto &val)
					{
						_map.emplace ((const void *) &val, pair_type {
							fieldNames[index++], tableName });
						return true;
					});
					return true;
				}, args...);
			}

			template <typename T>
			inline Field<T> operator () (const T &property)
			{
				const auto &result = Get (property);
				return Field<T> {
					std::move (result.first), result.second };
			}

			template <typename T>
			inline NullableField<T> operator () (const Nullable<T> &property)
			{
				const auto &result = Get (property);
				return NullableField<T> {
					std::move (result.first), result.second };
			}

		private:
			std::unordered_map<const void *, pair_type> _map;

			template <typename T>
			const pair_type &Get (const T &property)
			{
				try
				{
					return _map.at ((const void *) &property);
				}
				catch (...)
				{
					throw std::runtime_error ("No Such Field...");
				}
			}
		};

	private:
		BOT_ORM_Impl::SQLConnector _connector;

		template <typename Fn>
		inline void _Select (const std::string &sqlTarget,
							 const std::string &sqlFrom,
							 Fn fn)
		{
			_connector.Execute ("select " + sqlTarget +
								" from " + sqlFrom + ";", fn);
		}
	};

	namespace Helper
	{
		template <typename... Args>
		inline ORMapper::FieldExtractor FieldExtractor (
			const Args & ... args)
		{
			return ORMapper::FieldExtractor { args... };
		}

		inline auto Count ()
		{ return AggregateFunc<size_t> { "count (*)" }; }

		template <typename T>
		inline auto Count (const Field<T> &field)
		{ return AggregateFunc<T> { "count", field }; }

		template <typename T>
		inline auto Sum (const Field<T> &field)
		{ return AggregateFunc<T> { "sum", field }; }

		template <typename T>
		inline auto Avg (const Field<T> &field)
		{ return AggregateFunc<T> { "avg", field }; }

		template <typename T>
		inline auto Max (const Field<T> &field)
		{ return AggregateFunc<T> { "max", field }; }

		template <typename T>
		inline auto Min (const Field<T> &field)
		{ return AggregateFunc<T> { "min", field }; }
	}
}

#endif // !BOT_ORM_H