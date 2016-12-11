
// ORM Lite
// An ORM for SQLite in C++11
// https://github.com/BOT-Man-JL/ORM-Lite
// BOT Man, 2016

#ifndef BOT_ORM_H
#define BOT_ORM_H

// Container
#include <tuple>
#include <vector>
#include <list>
#include <string>
#include <unordered_map>

// Serialization
#include <sstream>

// Type Traits
#include <type_traits>

// std::nullptr_t
#include <cstddef>

// std::shared_ptr
#include <memory>

// for Field Name Extractor
#include <cctype>

// for SQL Connector
#include <thread>
#include <functional>

// SQLite 3 Dependency
#include "sqlite3.h"

// Public Macro

#define ORMAP(_TABLE_NAME_, ...)                          \
private:                                                  \
friend class BOT_ORM_Impl::InjectionHelper;               \
template <typename FN>                                    \
inline decltype (auto) __Accept (FN fn)                   \
{ return fn (__VA_ARGS__); }                              \
template <typename FN>                                    \
inline decltype (auto) __Accept (FN fn) const             \
{ return fn (__VA_ARGS__); }                              \
constexpr static const char *__FieldNames = #__VA_ARGS__; \
constexpr static const char *__TableName =  _TABLE_NAME_; \

// Private Macros

#define NO_ORMAP "Please Inject the Class with 'ORMAP' first"
#define BAD_TYPE "Only Support Integral, Floating Point and std::string"
#define NO_FIELD "No Such Field for current Extractor"
#define NULL_DESERIALIZE "Get Null Value for NOT Nullable Type"
#define NOT_SAME_TABLE "Fields are NOT from the Same Table"

// Nullable Template
// https://stackoverflow.com/questions/2537942/nullable-values-in-c/28811646#28811646

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
								std::nullptr_t);
		template <typename T2>
		friend bool operator== (std::nullptr_t,
								const Nullable<T2> &op);
	public:
		// Default or Null Construction
		Nullable ()
			: m_hasValue (false), m_value (T ()) {}
		Nullable (std::nullptr_t)
			: Nullable () {}

		// Null Assignment
		const Nullable<T> & operator= (std::nullptr_t)
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

	// == varialbe
	template <typename T2>
	inline bool operator== (const Nullable<T2> &op1,
							const Nullable<T2> &op2)
	{
		return op1.m_hasValue == op2.m_hasValue &&
			(!op1.m_hasValue || op1.m_value == op2.m_value);
	}

	// == value
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

	// == nullptr
	template <typename T2>
	inline bool operator== (const Nullable<T2> &op,
							std::nullptr_t)
	{
		return !op.m_hasValue;
	}
	template <typename T2>
	inline bool operator== (std::nullptr_t,
							const Nullable<T2> &op)
	{
		return !op.m_hasValue;
	}
}

// Helpers

namespace BOT_ORM_Impl
{
	// Naive SQL Driver (Todo: Improved Later)

	class SQLConnector
	{
	public:
		SQLConnector (const std::string &fileName)
		{
			if (sqlite3_open (fileName.c_str (), &db))
			{
				sqlite3_close (db);
				throw std::runtime_error (
					std::string ("SQL error: Can't open database '")
					+ sqlite3_errmsg (db) + "'");
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
				auto errStr = std::string ("SQL error: '") + zErrMsg
					+ "' at '" + cmd + "'";
				sqlite3_free (zErrMsg);
				throw std::runtime_error (errStr);
			}
		}

	private:
		sqlite3 *db;
		static inline void _callback (int, char **, char **) { return; }
	};

	// Helper - Serialize

	template <typename T>
	inline bool SerializeValue (
		std::ostream &os,
		const T &value)
	{
		os << value;
		return true;
	}

	template <>
	inline bool SerializeValue <std::string> (
		std::ostream &os,
		const std::string &value)
	{
		os << "'" << value << "'";
		return true;
	}

	template <typename T>
	inline bool SerializeValue (
		std::ostream &os,
		const BOT_ORM::Nullable<T> &value)
	{
		if (value == nullptr) return false;
		return SerializeValue (os, value.Value ());
	}

	// Helper - Deserialize

	template <typename T>
	inline void DeserializeValue (
		T &property, const char *value)
	{
		if (value)
			std::istringstream { value } >> property;
		else
			throw std::runtime_error (NULL_DESERIALIZE);
	}

	template <>
	inline void DeserializeValue <std::string> (
		std::string &property, const char *value)
	{
		if (value)
			property = value;
		else
			throw std::runtime_error (NULL_DESERIALIZE);
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

	// Injection Helper

	class InjectionHelper
	{
		static std::vector<std::string> ExtractFieldName (std::string input)
		{
			std::vector<std::string> ret;
			std::string tmpStr;

			for (const auto &ch : std::move (input) + ",")
			{
				if (isalnum (ch) || ch == '_')
					tmpStr += ch;
				else if (ch == ',')
				{
					if (!tmpStr.empty ()) ret.push_back (tmpStr);
					tmpStr.clear ();
				}
			}
			return ret;
		};

	public:

		// Checking Injection
		template <typename T> class HasInjected
		{
			template <typename...> struct void_t_Tester { using type = void; };
			template <typename... _Types>
			using void_t = typename void_t_Tester<_Types...>::type;

			template <typename, typename = void_t<>>
			struct Test : std::false_type {};
			template <typename U>
			struct Test <U, void_t<decltype (U::__TableName)>>
				: std::true_type {};

		public:
			static constexpr bool value = Test<T>::value;
		};

		// Proxy Function
		template <typename C, typename Fn>
		static inline decltype (auto) Visit (C &obj, Fn fn)
		{
			return obj.__Accept (fn);
		}

		// Field Name Proxy
		template <typename C>
		static inline const std::vector<std::string> &FieldNames (const C &)
		{
			static auto fieldNames = ExtractFieldName (C::__FieldNames);
			return fieldNames;
		}

		// Table Name Proxy
		template <typename C>
		static inline const std::string &TableName (const C &)
		{
			static std::string tableName { C::__TableName };
			return tableName;
		}
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

			inline SetExpr operator && (const SetExpr &right) const
			{ return SetExpr { expr + "," + right.expr }; }

		protected:
			std::string expr;
		};

		// Selectable

		template <typename T>
		struct Selectable
		{
			std::string fieldName;
			const std::string *tableName;

			Selectable (std::string _fieldName,
						const std::string *_tableName)
				: fieldName (std::move (_fieldName)), tableName (_tableName)
			{}
		};

		// Field : Selectable

		template <typename T>
		struct Field : public Selectable<T>
		{
			Field (std::string _fieldName,
				   const std::string *_tableName)
				: Selectable<T> (std::move (_fieldName), _tableName) {}

			inline SetExpr operator = (T value)
			{
				std::ostringstream os;
				BOT_ORM_Impl::SerializeValue (
					os << this->fieldName << "=", value);
				return SetExpr { os.str () };
			}
		};

		// Nullable Field : Field : Selectable

		template <typename T>
		struct NullableField : public Field<T>
		{
			NullableField (std::string _fieldName,
						   const std::string *_tableName)
				: Field<T> (std::move (_fieldName), _tableName) {}

			inline SetExpr operator = (T value)
			{
				std::ostringstream os;
				BOT_ORM_Impl::SerializeValue (
					os << this->fieldName << "=", value);
				return SetExpr { os.str () };
			}

			inline SetExpr operator = (std::nullptr_t)
			{ return SetExpr { this->fieldName + "=null" }; }
		};

		// Aggregate Function : Selectable

		template <typename T>
		struct Aggregate : public Selectable<T>
		{
			Aggregate (std::string function)
				: Selectable<T> (std::move (function), nullptr) {}

			Aggregate (std::string function, const Field<T> &field)
				: Selectable<T> (std::move (function) + "(" +
								 *(field.tableName) + "." +
								 field.fieldName + ")", nullptr)
			{}
		};

		// Expr

		struct Expr
		{
			template <typename T>
			Expr (const Selectable<T> &field,
				  std::string op_val)
				: exprs { { field.fieldName + op_val, field.tableName } }
			{}

			template <typename T>
			Expr (const Selectable<T> &field,
				  std::string op, T value)
			{
				std::ostringstream os;
				BOT_ORM_Impl::SerializeValue (
					os << field.fieldName << op, value);
				exprs.emplace_back (os.str (), field.tableName);
			}

			template <typename T>
			Expr (const Field<T> &field1,
				  std::string op,
				  const Field<T> &field2)
				: exprs
			{
				{ field1.fieldName, field1.tableName },
				{ std::move (op), nullptr },
				{ field2.fieldName, field2.tableName }
			}
			{}

			std::string ToString (bool withTableName) const
			{
				std::ostringstream out;
				for (const auto &expr : exprs)
				{
					if (withTableName && expr.second != nullptr)
						out << *(expr.second) << ".";
					out << expr.first;
				}
				return out.str ();
			}

			inline Expr operator && (const Expr &right) const
			{ return And_Or (right, " and "); }
			inline Expr operator || (const Expr &right) const
			{ return And_Or (right, " or "); }

		protected:
			std::list<std::pair<std::string, const std::string *>> exprs;

			Expr And_Or (const Expr &right, std::string logOp) const
			{
				auto ret = *this;
				auto rigthExprs = right.exprs;
				ret.exprs.emplace_front ("(", nullptr);
				ret.exprs.emplace_back (std::move (logOp), nullptr);
				ret.exprs.splice (ret.exprs.cend (),
								  std::move (rigthExprs));
				ret.exprs.emplace_back (")", nullptr);
				return ret;
			}
		};

		// Field / Aggregate ? Value

		template <typename T>
		inline Expr operator == (const Selectable<T> &op, T value)
		{ return Expr (op, "=", std::move (value)); }

		template <typename T>
		inline Expr operator != (const Selectable<T> &op, T value)
		{ return Expr (op, "!=", std::move (value)); }

		template <typename T>
		inline Expr operator > (const Selectable<T> &op, T value)
		{ return Expr (op, ">", std::move (value)); }

		template <typename T>
		inline Expr operator >= (const Selectable<T> &op, T value)
		{ return Expr (op, ">=", std::move (value)); }

		template <typename T>
		inline Expr operator < (const Selectable<T> &op, T value)
		{ return Expr (op, "<", std::move (value)); }

		template <typename T>
		inline Expr operator <= (const Selectable<T> &op, T value)
		{ return Expr (op, "<=", std::move (value)); }

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

		// Nullable Field == / != nullptr

		template <typename T>
		inline Expr operator == (const NullableField<T> &op, std::nullptr_t)
		{ return Expr { op, " is null" }; }

		template <typename T>
		inline Expr operator != (const NullableField<T> &op, std::nullptr_t)
		{ return Expr { op, " is not null" }; }

		// String Field & / | std::string

		inline Expr operator & (const Field<std::string> &field,
								std::string val)
		{
			return Expr (field, " like ", std::move (val));
		}

		inline Expr operator | (const Field<std::string> &field,
								std::string val)
		{
			return Expr (field, " not like ", std::move (val));
		}

		// Aggregate Function Helpers

		inline auto Count ()
		{ return Aggregate<size_t> { "count (*)" }; }

		template <typename T>
		inline auto Count (const Field<T> &field)
		{ return Aggregate<T> { "count", field }; }

		template <typename T>
		inline auto Sum (const Field<T> &field)
		{ return Aggregate<T> { "sum", field }; }

		template <typename T>
		inline auto Avg (const Field<T> &field)
		{ return Aggregate<T> { "avg", field }; }

		template <typename T>
		inline auto Max (const Field<T> &field)
		{ return Aggregate<T> { "max", field }; }

		template <typename T>
		inline auto Min (const Field<T> &field)
		{ return Aggregate<T> { "min", field }; }
	}

	class ORMapper;

	class Constraint
	{
	protected:
		std::string constraint;
		std::string field;

		Constraint (std::string &&_constraint,
					std::string _field = std::string {})
			: constraint (_constraint), field (std::move (_field))
		{}

		friend class ORMapper;

	public:
		struct CompositeField
		{
			std::string fieldName;
			const std::string *tableName;

			template <typename... Fields>
			CompositeField (const Fields & ... args)
				: tableName (nullptr)
			{
				// Unpacking Tricks :-)
				using expander = int[];
				(void) expander { 0, (Extract (args), 0)... };
			}

		private:
			template <typename T>
			void Extract (const Expression::Field<T> &field)
			{
				if (tableName != nullptr && tableName != field.tableName)
					throw std::runtime_error (NOT_SAME_TABLE);
				tableName = field.tableName;
				if (fieldName.empty ())
					fieldName = field.fieldName;
				else
					fieldName = field.fieldName + "," + fieldName;
			}
		};

		template <typename T>
		static inline Constraint Default (
			const Expression::Field<T> &field,
			const T &value)
		{
			std::ostringstream os;
			BOT_ORM_Impl::SerializeValue (os << " default ", value);
			return Constraint { os.str (), field.fieldName };
		}

		static inline Constraint Check (
			const Expression::Expr &expr)
		{
			return Constraint { "check (" + expr.ToString (false) + ")" };
		}

		template <typename T>
		static inline Constraint Unique (
			const Expression::Field<T> &field)
		{
			return Constraint { "unique (" + field.fieldName + ")" };
		}

		static inline Constraint Unique (
			const CompositeField &fields)
		{
			return Constraint { "unique (" + fields.fieldName + ")" };
		}

		template <typename T>
		static inline Constraint Reference (
			const Expression::Field<T> &field,
			const Expression::Field<T> &refered)
		{
			return Constraint {
				std::string ("foreign key (") + field.fieldName +
				") references " + *(refered.tableName) +
				"(" + refered.fieldName + ")" };
		}

		static inline Constraint Reference (
			const CompositeField &field,
			const CompositeField &refered)
		{
			return Constraint {
				std::string ("foreign key (") + field.fieldName +
				") references " + *(refered.tableName) +
				"(" + refered.fieldName + ")" };
		}
	};

	template <typename T>
	class Queryable;
}

namespace BOT_ORM_Impl
{
	// Why Remove QueryableHelper from Query?
	// Query is a template but the Helper can be shared

	class QueryableHelper
	{
		template <typename T>
		using Nullable = BOT_ORM::Nullable<T>;

		template <typename T>
		using Selectable = BOT_ORM::Expression::Selectable<T>;

	protected:
		template <typename T>
		friend class BOT_ORM::Queryable;

		// #1 Tuple Visitor

		// About 'using expander = int[]'
		// http://stackoverflow.com/questions/26902633/how-to-iterate-over-a-tuple-in-c-11/26902803#26902803
		// - To avoid the unspecified order,
		//   brace-enclosed initializer lists can be used,
		//   which guarantee strict left-to-right order of evaluation.
		// - To avoid the need for a not void return type,
		//   the comma operator can be used to
		//   always yield 1 in each expansion element.

		// Apply 'Fn' to each of element of Tuple
		template <typename Fn, typename TupleType, std::size_t... I>
		static inline void TupleVisit_Impl (
			TupleType &tuple, Fn fn, std::index_sequence<I...>)
		{
			// Unpacking Tricks :-)
			using expander = int[];
			(void) expander { 0, ((void) fn (std::get<I> (tuple)), 0)... };
		}

		// Produce the 'index_sequence' for 'tuple'
		template <typename Fn, typename TupleType>
		static inline void TupleVisit (TupleType &tuple, Fn fn)
		{
			constexpr auto size = std::tuple_size<TupleType>::value;
			return TupleVisit_Impl (tuple, fn,
									std::make_index_sequence<size> {});
		}

		// #2 Join To Tuple

		// Type To Nullable
		// Get Nullable Type Wrappers for Non-nullable Types
		template <typename T> struct TypeToNullable
		{
			using type = Nullable<T>;
		};
		template <typename T> struct TypeToNullable <Nullable<T>>
		{
			using type = Nullable<T>;
		};

		// Fields To Nullable
		// Apply 'TypeToNullable' to each element of Fields
		template <typename... Args>
		static inline auto FieldsToNullable (Args...)
		{
			// Unpacking Tricks :-)
			// Expand each of 'Args'
			// with 'TypeToNullable_t<...>' as a sequence
			return std::tuple<
				typename TypeToNullable<Args>::type...
			> {};
		}

		// QueryResult To Nullable
		template <typename C>
		static inline auto QueryResultToTuple (const C &arg)
		{
			return BOT_ORM_Impl::InjectionHelper::Visit (
				arg, [] (auto ... args)
			{
				return FieldsToNullable (args...);
			});
		}

		template <typename... Args>
		static inline auto QueryResultToTuple (
			const std::tuple<Args...>& t)
		{
			// FieldsToNullable is not necessary: Nullable already
			return t;
		}

		// Construct Tuple from QueryResult List
		template <typename... Args>
		static inline auto JoinToTuple (const Args & ... args)
		{
			// Unpacking Tricks :-)
			return decltype (std::tuple_cat (
				QueryResultToTuple (args)...
			)) {};
		}

		// #3 Select To Tuple

		// Selectable To Tuple
		template <typename T>
		static inline auto SelectableToTuple (const Selectable<T> &)
		{
			// Notes: Only 'const Selectable<T> &' will overload...
			return Nullable<T> {};
		}

		// Construct Tuple from Selectable<T> List
		template <typename... Args>
		static inline auto SelectToTuple (const Args & ... args)
		{
			// Unpacking Tricks :-)
			return std::tuple <
				decltype (SelectableToTuple (args))...
			> {};
		}

		// #4 Field To SQL

		// Return Field Strings for GroupBy, OrderBy and Select
		template <typename T>
		static inline std::string FieldToSql (const Selectable<T> &op)
		{
			if (op.tableName) return (*op.tableName) + "." + op.fieldName;
			else return op.fieldName;
		}
		template <typename T, typename... Args>
		static inline std::string FieldToSql (const Selectable<T> &arg,
											  const Args & ... args)
		{
			return FieldToSql (arg) + "," + FieldToSql (args...);
		}
	};
}

namespace BOT_ORM
{
	// Queryable

	template <typename QueryResult>
	class Queryable
	{
		template <typename C>
		using HasInjected =
			BOT_ORM_Impl::InjectionHelper::HasInjected<C>;

	protected:
		std::shared_ptr<BOT_ORM_Impl::SQLConnector> _connector;
		QueryResult _queryHelper;

		std::string _sqlFrom;
		std::string _sqlSelect;
		std::string _sqlTarget;

		std::string _sqlWhere;
		std::string _sqlGroupBy;
		std::string _sqlHaving;

		std::string _sqlOrderBy;
		std::string _sqlLimit;
		std::string _sqlOffset;

		Queryable (
			std::shared_ptr<BOT_ORM_Impl::SQLConnector> connector,
			QueryResult queryHelper,
			std::string sqlFrom,
			std::string sqlSelect = "select ",
			std::string sqlTarget = "*",
			std::string sqlWhere = std::string {},
			std::string sqlGroupBy = std::string {},
			std::string sqlHaving = std::string {},
			std::string sqlOrderBy = std::string {},
			std::string sqlLimit = std::string {},
			std::string sqlOffset = std::string {})
			:
			_connector (std::move (connector)),
			_queryHelper (std::move (queryHelper)),
			_sqlFrom (std::move (sqlFrom)),
			_sqlSelect (std::move (sqlSelect)),
			_sqlTarget (std::move (sqlTarget)),
			_sqlWhere (std::move (sqlWhere)),
			_sqlGroupBy (std::move (sqlGroupBy)),
			_sqlHaving (std::move (sqlHaving)),
			_sqlOrderBy (std::move (sqlOrderBy)),
			_sqlLimit (std::move (sqlLimit)),
			_sqlOffset (std::move (sqlOffset))
		{}

		template <typename Q> friend class Queryable;
		friend class ORMapper;

	public:
		// Distinct
		inline Queryable Distinct () const
		{
			auto ret = *this;
			ret._sqlSelect = "select distinct ";
			return ret;
		}

		// Where
		inline Queryable Where (const Expression::Expr &expr) const
		{
			auto ret = *this;
			ret._sqlWhere = " where (" + expr.ToString (true) + ")";
			return ret;
		}

		// Group By
		template <typename T>
		inline Queryable GroupBy (const Expression::Field<T> &field) const
		{
			auto ret = *this;
			ret._sqlGroupBy = " group by " +
				BOT_ORM_Impl::QueryableHelper::FieldToSql (field);
			return ret;
		}
		inline Queryable Having (const Expression::Expr &expr) const
		{
			auto ret = *this;
			ret._sqlHaving = " having " + expr.ToString (true);
			return ret;
		}

		// Limit and Offset
		inline Queryable Take (size_t count) const
		{
			auto ret = *this;
			ret._sqlLimit = " limit " + std::to_string (count);
			return ret;
		}
		inline Queryable Skip (size_t count) const
		{
			auto ret = *this;
			if (ret._sqlLimit.empty ())
				ret._sqlLimit = " limit ~0";  // ~0 is a trick :-)
			ret._sqlOffset = " offset " + std::to_string (count);
			return ret;
		}

		// Order By
		template <typename T>
		inline Queryable OrderBy (
			const Expression::Field<T> &field) const
		{
			auto ret = *this;
			if (ret._sqlOrderBy.empty ())
				ret._sqlOrderBy = " order by " +
				BOT_ORM_Impl::QueryableHelper::FieldToSql (field);
			else
				ret._sqlOrderBy += "," +
				BOT_ORM_Impl::QueryableHelper::FieldToSql (field);
			return ret;
		}
		template <typename T>
		inline Queryable OrderByDescending (
			const Expression::Field<T> &field) const
		{
			auto ret = *this;
			if (ret._sqlOrderBy.empty ())
				ret._sqlOrderBy = " order by " +
				BOT_ORM_Impl::QueryableHelper::FieldToSql (field) + " desc";
			else
				ret._sqlOrderBy += "," +
				BOT_ORM_Impl::QueryableHelper::FieldToSql (field) + " desc";
			return ret;
		}

		// Select
		template <typename... Args>
		inline auto Select (const Args & ... args) const
		{
			return _NewQuery (
				BOT_ORM_Impl::QueryableHelper::FieldToSql (args...),
				_sqlFrom,
				BOT_ORM_Impl::QueryableHelper::SelectToTuple (args...)
			);
		}

		// Join
		template <typename C>
		inline auto Join (const C &queryHelper2,
						  const Expression::Expr &onExpr,
						  std::enable_if_t<
						  !HasInjected<C>::value>
						  * = nullptr) const
		{
			static_assert (HasInjected<C>::value, NO_ORMAP);
		}
		template <typename C>
		inline auto Join (const C &queryHelper2,
						  const Expression::Expr &onExpr,
						  std::enable_if_t<
						  HasInjected<C>::value>
						  * = nullptr) const
		{
			return _NewJoinQuery (queryHelper2, onExpr, " join ");
		}

		// Left Join
		template <typename C>
		inline auto LeftJoin (const C &queryHelper2,
							  const Expression::Expr &onExpr,
							  std::enable_if_t<
							  !HasInjected<C>::value>
							  * = nullptr) const
		{
			static_assert (HasInjected<C>::value, NO_ORMAP);
		}
		template <typename C>
		inline auto LeftJoin (const C &queryHelper2,
							  const Expression::Expr &onExpr,
							  std::enable_if_t<
							  HasInjected<C>::value>
							  * = nullptr) const
		{
			return _NewJoinQuery (queryHelper2, onExpr, " left join ");
		}

		// Compound Select
		inline Queryable Union (const Queryable &queryable) const
		{ return _NewCompoundQuery (queryable, " union "); }
		inline Queryable UnionAll (Queryable queryable) const
		{ return _NewCompoundQuery (queryable, " union all "); }
		inline Queryable Intersect (Queryable queryable) const
		{ return _NewCompoundQuery (queryable, " intersect "); }
		inline Queryable Except (Queryable queryable) const
		{ return _NewCompoundQuery (queryable, " excpet "); }

		// Get Result
		template <typename T>
		Nullable<T> Select (const Expression::Aggregate<T> &agg) const
		{
			Nullable<T> ret;
			_connector->Execute (_sqlSelect + agg.fieldName +
								 _GetFromSql () + _GetLimit () + ";",
								 [&] (int argc, char **argv, char **)
			{
				BOT_ORM_Impl::DeserializeValue (ret, argv[0]);
			});
			return ret;
		}

		std::vector<QueryResult> ToVector () const
		{
			std::vector<QueryResult> ret;
			_Select (_queryHelper, ret);
			return ret;
		}
		std::list<QueryResult> ToList () const
		{
			std::list<QueryResult> ret;
			_Select (_queryHelper, ret);
			return ret;
		}

	protected:
		// Return FROM part for Query
		inline std::string _GetFromSql () const
		{ return _sqlFrom + _sqlWhere + _sqlGroupBy + _sqlHaving; }

		// Return ORDER BY & LIMIT part for Query
		inline std::string _GetLimit () const
		{ return _sqlOrderBy + _sqlLimit + _sqlOffset; }

		// Return a new Queryable Object
		template <typename... Args>
		inline auto _NewQuery (std::string sqlTarget,
							   std::string sqlFrom,
							   std::tuple<Args...> &&newQueryHelper) const
		{
			return Queryable<std::tuple<Args...>> (
				_connector, newQueryHelper,
				std::move (sqlFrom),
				_sqlSelect, std::move (sqlTarget),
				_sqlWhere, _sqlGroupBy, _sqlHaving,
				_sqlOrderBy, _sqlLimit, _sqlOffset);
		}

		// Return a new Join Queryable Object
		template <typename C>
		inline auto _NewJoinQuery (const C &queryHelper2,
								   const Expression::Expr &onExpr,
								   std::string joinStr) const
		{
			return _NewQuery (
				_sqlTarget,
				_sqlFrom + std::move (joinStr) +
				BOT_ORM_Impl::InjectionHelper::TableName (queryHelper2) +
				" on " + onExpr.ToString (true),
				BOT_ORM_Impl::QueryableHelper::JoinToTuple (
					_queryHelper, queryHelper2));
		}

		// Return a new Compound Queryable Object
		auto _NewCompoundQuery (const Queryable &queryable,
								std::string compoundStr) const
		{
			auto ret = *this;
			ret._sqlFrom = ret._GetFromSql () +
				std::move (compoundStr) +
				queryable._sqlSelect + queryable._sqlTarget +
				queryable._GetFromSql ();
			ret._sqlWhere.clear ();
			ret._sqlGroupBy.clear ();
			ret._sqlHaving.clear ();
			return ret;
		}

		// Select for Normal Objects
		template <typename C, typename Out>
		void _Select (const C &, Out &out) const
		{
			auto copy = _queryHelper;
			_connector->Execute (_sqlSelect + _sqlTarget +
								 _GetFromSql () + _GetLimit () + ";",
								 [&] (int, char **argv, char **)
			{
				BOT_ORM_Impl::InjectionHelper::Visit (
					copy, [&argv] (auto & ... args)
				{
					size_t index = 0;
					// Unpacking Tricks :-)
					using expander = int[];
					(void) expander
					{
						0, (BOT_ORM_Impl::DeserializeValue (
							args, argv[index++]
						), 0)...
					};
				});
				out.push_back (copy);
			});
		}

		// Select for Tuples
		template <typename Out, typename... Args>
		void _Select (const std::tuple<Args...> &, Out &out) const
		{
			auto copy = _queryHelper;
			_connector->Execute (_sqlSelect + _sqlTarget +
								 _GetFromSql () + _GetLimit () + ";",
								 [&] (int, char **argv, char **)
			{
				size_t index = 0;
				BOT_ORM_Impl::QueryableHelper::TupleVisit (
					copy, [&argv, &index] (auto &val)
				{
					BOT_ORM_Impl::DeserializeValue (val, argv[index++]);
				});
				out.push_back (copy);
			});
		}
	};

	// ORMapper

	class ORMapper
	{
		template <typename C>
		using HasInjected =
			BOT_ORM_Impl::InjectionHelper::HasInjected<C>;

	public:
		ORMapper (const std::string &connectionString)
			: _connector (
				std::make_shared<BOT_ORM_Impl::SQLConnector> (
					connectionString))
		{
			_connector->Execute ("PRAGMA foreign_keys = ON;");
		}

		template <typename Fn>
		void Transaction (Fn fn)
		{
			try
			{
				_connector->Execute ("begin transaction;");
				fn ();
				_connector->Execute ("commit transaction;");
			}
			catch (...)
			{
				_connector->Execute ("rollback transaction;");
				throw;
			}
		}

		template <typename C, typename... Args>
		std::enable_if_t<!HasInjected<C>::value>
			CreateTbl (const C &entity, const Args & ... args)
		{
			static_assert (HasInjected<C>::value, NO_ORMAP);
		}
		template <typename C, typename... Args>
		std::enable_if_t<HasInjected<C>::value>
			CreateTbl (const C &entity, const Args & ... args)
		{
			const auto &fieldNames =
				BOT_ORM_Impl::InjectionHelper::FieldNames (entity);

			std::unordered_map<std::string, std::string> fieldFixes;

			BOT_ORM_Impl::InjectionHelper::Visit (
				entity, [&fieldNames, &fieldFixes] (const auto & ... args)
			{
				size_t index = 0;
				// Unpacking Tricks :-)
				using expander = int[];
				(void) expander
				{
					0, (fieldFixes.emplace (
						fieldNames[index++], _TypeString (args)
					), 0)...
				};
			});
			fieldFixes[fieldNames[0]] += " primary key";

			std::string tableFixes;
			_GetConstraints (tableFixes, fieldFixes, args...);

			std::string strFmt;
			for (const auto &field : fieldNames)
				strFmt += field + fieldFixes[field] + ",";
			strFmt += std::move (tableFixes);
			strFmt.pop_back ();

			_connector->Execute (
				"create table " +
				BOT_ORM_Impl::InjectionHelper::TableName (entity) +
				"(" + strFmt + ");");
		}

		template <typename C>
		std::enable_if_t<!HasInjected<C>::value>
			DropTbl (const C &entity)
		{
			static_assert (HasInjected<C>::value, NO_ORMAP);
		}
		template <typename C>
		std::enable_if_t<HasInjected<C>::value>
			DropTbl (const C &entity)
		{
			_connector->Execute (
				"drop table " +
				BOT_ORM_Impl::InjectionHelper::TableName (entity) +
				";");
		}

		template <typename C>
		std::enable_if_t<!HasInjected<C>::value>
			Insert (const C &entity, bool withId = true)
		{
			static_assert (HasInjected<C>::value, NO_ORMAP);
		}
		template <typename C>
		std::enable_if_t<HasInjected<C>::value>
			Insert (const C &entity, bool withId = true)
		{
			std::ostringstream os;
			_GetInsert (os, entity, withId);
			_connector->Execute (os.str ());
		}

		template <typename In, typename C = typename In::value_type>
		std::enable_if_t<!HasInjected<C>::value>
			InsertRange (const In &entities, bool withId = true)
		{
			static_assert (HasInjected<C>::value, NO_ORMAP);
		}
		template <typename In, typename C = typename In::value_type>
		std::enable_if_t<HasInjected<C>::value>
			InsertRange (const In &entities, bool withId = true)
		{
			std::ostringstream os;
			auto anyEntity = false;
			for (const auto &entity : entities)
			{
				_GetInsert (os, entity, withId);
				anyEntity = true;
			}
			if (anyEntity)
				_connector->Execute (os.str ());
		}

		template <typename C>
		std::enable_if_t<!HasInjected<C>::value>
			Update (const C &entity)
		{
			static_assert (HasInjected<C>::value, NO_ORMAP);
		}
		template <typename C>
		std::enable_if_t<HasInjected<C>::value>
			Update (const C &entity)
		{
			std::ostringstream os;
			if (_GetUpdate (os, entity))
				_connector->Execute (os.str ());
		}

		template <typename In, typename C = typename In::value_type>
		std::enable_if_t<!HasInjected<C>::value>
			UpdateRange (const In &entities)
		{
			static_assert (HasInjected<C>::value, NO_ORMAP);
		}
		template <typename In, typename C = typename In::value_type>
		std::enable_if_t<HasInjected<C>::value>
			UpdateRange (const In &entities)
		{
			std::ostringstream os, osTmp;
			for (const auto &entity : entities)
				if (_GetUpdate (osTmp, entity))
				{
					os << osTmp.str ();
					osTmp.str (std::string {});  // Flush the previous data
				}
			auto sql = os.str ();
			if (!sql.empty ()) _connector->Execute (sql);
		}

		template <typename C>
		std::enable_if_t<!HasInjected<C>::value>
			Update (const C &entity,
					const Expression::SetExpr &setExpr,
					const Expression::Expr &whereExpr)
		{
			static_assert (HasInjected<C>::value, NO_ORMAP);
		}
		template <typename C>
		std::enable_if_t<HasInjected<C>::value>
			Update (const C &entity,
					const Expression::SetExpr &setExpr,
					const Expression::Expr &whereExpr)
		{
			_connector->Execute (
				"update " +
				BOT_ORM_Impl::InjectionHelper::TableName (entity) +
				" set " + setExpr.ToString () +
				" where " +
				whereExpr.ToString (false) + ";");
		}

		template <typename C>
		std::enable_if_t<!HasInjected<C>::value>
			Delete (const C &entity)
		{
			static_assert (HasInjected<C>::value, NO_ORMAP);
		}
		template <typename C>
		std::enable_if_t<HasInjected<C>::value>
			Delete (const C &entity)
		{
			const auto &fieldNames =
				BOT_ORM_Impl::InjectionHelper::FieldNames (entity);

			std::ostringstream os;
			os << "delete from "
				<< BOT_ORM_Impl::InjectionHelper::TableName (entity)
				<< " where " << fieldNames[0] << "=";

			// Primary Key
			BOT_ORM_Impl::InjectionHelper::Visit (
				entity, [&os] (const auto &primaryKey, const auto & ... dummy)
			{
				// Why 'dummy'?
				// It's an issue of gcc 5.4:
				//   'template argument deduction/substitution failed'
				//   if no 'dummy' literal (WTF) !!!
				if (!BOT_ORM_Impl::SerializeValue (os, primaryKey))
					os << "null";
			});
			os << ";";

			_connector->Execute (os.str ());
		}

		template <typename C>
		std::enable_if_t<!HasInjected<C>::value>
			Delete (const C &entity,
					const Expression::Expr &whereExpr)
		{
			static_assert (HasInjected<C>::value, NO_ORMAP);
		}
		template <typename C>
		std::enable_if_t<HasInjected<C>::value>
			Delete (const C &entity,
					const Expression::Expr &whereExpr)
		{
			_connector->Execute (
				"delete from " +
				BOT_ORM_Impl::InjectionHelper::TableName (entity) +
				" where " +
				whereExpr.ToString (false) + ";");
		}

		template <typename C>
		std::enable_if_t<!HasInjected<C>::value, Queryable<C>>
			Query (C queryHelper)
		{
			static_assert (HasInjected<C>::value, NO_ORMAP);
		}
		template <typename C>
		std::enable_if_t<HasInjected<C>::value, Queryable<C>>
			Query (C queryHelper)
		{
			return Queryable<C> (
				_connector,
				std::move (queryHelper),
				std::string (" from ") +
				BOT_ORM_Impl::InjectionHelper::TableName (queryHelper));
		}

	protected:
		std::shared_ptr<BOT_ORM_Impl::SQLConnector> _connector;

		template <typename T>
		static inline const char *_TypeString (const T &)
		{
			constexpr static const char *typeStr =
				(std::is_integral<T>::value &&
				 !std::is_same<std::remove_cv_t<T>, char>::value &&
				 !std::is_same<std::remove_cv_t<T>, wchar_t>::value &&
				 !std::is_same<std::remove_cv_t<T>, char16_t>::value &&
				 !std::is_same<std::remove_cv_t<T>, char32_t>::value &&
				 !std::is_same<std::remove_cv_t<T>, unsigned char>::value)
				? " integer not null"
				: (std::is_floating_point<T>::value)
				? " real not null"
				: (std::is_same<std::remove_cv_t<T>, std::string>::value)
				? " text not null"
				: nullptr;

			static_assert (typeStr != nullptr, BAD_TYPE);
			return typeStr;
		}

		template <typename T>
		static inline const char *_TypeString (const BOT_ORM::Nullable<T> &)
		{
			constexpr static const char *typeStr =
				(std::is_integral<T>::value &&
				 !std::is_same<std::remove_cv_t<T>, char>::value &&
				 !std::is_same<std::remove_cv_t<T>, wchar_t>::value &&
				 !std::is_same<std::remove_cv_t<T>, char16_t>::value &&
				 !std::is_same<std::remove_cv_t<T>, char32_t>::value &&
				 !std::is_same<std::remove_cv_t<T>, unsigned char>::value)
				? " integer"
				: (std::is_floating_point<T>::value)
				? " real"
				: (std::is_same<std::remove_cv_t<T>, std::string>::value)
				? " text"
				: nullptr;

			static_assert (typeStr != nullptr, BAD_TYPE);
			return typeStr;
		}

		static void _GetConstraints (
			std::string &,
			std::unordered_map<std::string, std::string> &)
		{}

		template <typename... Args>
		static void _GetConstraints (
			std::string &tableFixes,
			std::unordered_map<std::string, std::string> &fieldFixes,
			const Constraint &constraint,
			const Args & ... args)
		{
			if (!constraint.field.empty ())
				fieldFixes[constraint.field] += constraint.constraint;
			else
				tableFixes += constraint.constraint + ",";
			_GetConstraints (tableFixes, fieldFixes, args...);
		}

		template <typename C>
		static inline void _GetInsert (
			std::ostream &os, const C &entity, bool withId)
		{
			BOT_ORM_Impl::InjectionHelper::Visit (
				entity, [&os, &entity, withId] (
					const auto &primaryKey, const auto & ... args)
			{
				const auto &fieldNames =
					BOT_ORM_Impl::InjectionHelper::FieldNames (entity);

				os << "insert into "
					<< BOT_ORM_Impl::InjectionHelper::TableName (entity)
					<< "(";

				std::ostringstream osVal;

				// Avoid Bad Eating of ,
				bool anyField = false;

				auto serializeField =
					[&fieldNames, &os, &osVal, &anyField] (
						const auto &val, size_t index)
				{
					if (BOT_ORM_Impl::SerializeValue (osVal, val))
					{
						os << fieldNames[index] << ",";
						osVal << ",";
						anyField = true;
					}
				};

				// Priamry Key
				if (withId &&
					BOT_ORM_Impl::SerializeValue (osVal, primaryKey))
				{
					os << fieldNames[0] << ",";
					osVal << ",";
					anyField = true;
				}

				// The Rest
				size_t index = 1;

				// Unpacking Tricks :-)
				using expander = int[];
				(void) expander
				{
					0, (serializeField (args, index++), 0)...
				};

				if (anyField)
				{
					os.seekp (os.tellp () - std::streamoff (1));
					osVal.seekp (osVal.tellp () - std::streamoff (1));
				}
				else  // Fix for No Field for Insert...
				{
					os << fieldNames[0];
					osVal << "null";
				}

				osVal << ");";  // To Enable Seekp...
				os << ") values (" << osVal.str ();
			});
		}

		template <typename C>
		static inline bool _GetUpdate (
			std::ostream &os, const C &entity)
		{
			return BOT_ORM_Impl::InjectionHelper::Visit (
				entity, [&os, &entity] (
					const auto &primaryKey, const auto & ... args)
			{
				if (sizeof... (args) == 0)
					return false;

				const auto &fieldNames =
					BOT_ORM_Impl::InjectionHelper::FieldNames (entity);
				os << "update "
					<< BOT_ORM_Impl::InjectionHelper::TableName (entity)
					<< " set ";

				auto serializeField = [&fieldNames, &os] (
					const auto &val, size_t index)
				{
					os << fieldNames[index] << "=";
					if (!BOT_ORM_Impl::SerializeValue (os, val))
						os << "null";
					os << ",";
				};

				// The Rest
				size_t index = 1;

				// Unpacking Tricks :-)
				using expander = int[];
				(void) expander
				{
					0, (serializeField (args, index++), 0)...
				};

				os.seekp (os.tellp () - std::streamoff (1));

				// Primary Key
				os << " where " << fieldNames[0] << "=";
				if (!BOT_ORM_Impl::SerializeValue (os, primaryKey))
					os << "null";

				os << ";";
				return true;
			});
		}
	};

	// Field Extractor

	class FieldExtractor
	{
		using pair_type = std::pair<
			const std::string &,
			const std::string &>;

		template <typename C>
		using HasInjected =
			BOT_ORM_Impl::InjectionHelper::HasInjected<C>;

		template <typename C>
		std::enable_if_t<!HasInjected<C>::value>
			Extract (const C &helper)
		{
			static_assert (HasInjected<C>::value, NO_ORMAP);
		}
		template <typename C>
		std::enable_if_t<HasInjected<C>::value>
			Extract (const C &helper)
		{
			BOT_ORM_Impl::InjectionHelper::Visit (
				helper, [this, &helper] (
					const auto & ... args)
			{
				const auto &fieldNames =
					BOT_ORM_Impl::InjectionHelper::FieldNames (helper);
				const auto &tableName =
					BOT_ORM_Impl::InjectionHelper::TableName (helper);

				size_t index = 0;
				// Unpacking Tricks :-)
				using expander = int[];
				(void) expander
				{
					0, (_map.emplace (
						(const void *) &args,
						pair_type { fieldNames[index++], tableName }
					), 0)...
				};
			});
		}

	public:
		template <typename... Classes>
		FieldExtractor (const Classes & ... args)
		{
			// Unpacking Tricks :-)
			using expander = int[];
			(void) expander { 0, (Extract (args), 0)... };
		}

		template <typename T>
		inline Expression::Field<T> operator () (
			const T &field) const
		{
			const auto &result = Get (field);
			return Expression::Field<T> {
				std::move (result.first), &result.second };
		}

		template <typename T>
		inline Expression::NullableField<T> operator () (
			const Nullable<T> &field) const
		{
			const auto &result = Get (field);
			return Expression::NullableField<T> {
				std::move (result.first), &result.second };
		}

	private:
		std::unordered_map<const void *, pair_type> _map;

		template <typename T>
		const pair_type &Get (const T &field) const
		{
			try { return _map.at ((const void *) &field); }
			catch (...) { throw std::runtime_error (NO_FIELD); }
		}
	};
}

// Clear Intra Macros
#undef NO_ORMAP
#undef BAD_TYPE
#undef NO_FIELD
#undef NULL_DESERIALIZE
#undef NOT_SAME_TABLE

#endif // !BOT_ORM_H