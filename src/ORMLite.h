
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
friend class BOT_ORM::FieldExtractor;                     \
template <typename T>                                     \
friend class BOT_ORM::Queryable;                          \
friend class BOT_ORM_Impl::QueryableHelper;               \
template <typename T>                                     \
friend class BOT_ORM_Impl::HasInjected;                   \
template <typename FN>                                    \
void __Accept (FN fn)                                     \
{                                                         \
    BOT_ORM_Impl::InjectedHelper::Visit (fn, __VA_ARGS__);\
}                                                         \
template <typename FN>                                    \
void __Accept (FN fn) const                               \
{                                                         \
    BOT_ORM_Impl::InjectedHelper::Visit (fn, __VA_ARGS__);\
}                                                         \
auto __Tuple () const                                     \
{                                                         \
    return std::make_tuple (__VA_ARGS__);                 \
}                                                         \
static const std::vector<std::string> &__FieldNames ()    \
{                                                         \
    static const std::vector<std::string> _fieldNames {   \
        BOT_ORM_Impl::InjectedHelper::ExtractFieldName (  \
        #__VA_ARGS__) };                                  \
    return _fieldNames;                                   \
}                                                         \
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
	// Naive SQL Driver ... (Todo: Improved Later)

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
		static void _callback (int argc, char **argv, char **azColName)
		{ return; }
	};

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

	// Injected Helper

	struct InjectedHelper
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
					ret.push_back (tmpStr);
					tmpStr.clear ();
				}
			}
			return ret;
		};

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
			const char *prefixStr;

			Selectable (std::string &&_fieldName, const char *_prefixStr)
				: fieldName (_fieldName), prefixStr (_prefixStr) {}
		};

		// Field : Selectable

		template <typename T>
		struct Field : public Selectable<T>
		{
			Field (std::string fieldName,
				   const char *tableName)
				: Selectable<T> { std::move (fieldName), tableName } {}

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

			inline SetExpr operator = (std::nullptr_t)
			{ return SetExpr { this->fieldName + "=null" }; }
		};

		// Aggregate Function : Selectable

		template <typename T>
		struct Aggregate : public Selectable<T>
		{
			Aggregate (std::string function)
				: Selectable<T> { std::move (function), nullptr } {}

			Aggregate (std::string function, const Field<T> &field)
				: Selectable<T> { function + "(" + field.prefixStr +
				"." + field.fieldName + ")", nullptr } {}
		};

		// Expr

		struct Expr
		{
			template <typename T>
			Expr (const Selectable<T> &field,
				  std::string op_val)
				: exprs { { field.fieldName + op_val, field.prefixStr } }
			{}

			template <typename T>
			Expr (const Selectable<T> &field,
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

			std::string ToString (bool withTableName) const
			{
				std::ostringstream out;
				for (const auto &expr : exprs)
				{
					if (withTableName && expr.second != nullptr)
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
		std::string field;
		std::string constraint;

		Constraint (std::string _field, std::string _constraint)
			: field (_field), constraint (_constraint) {}

		friend class ORMapper;

	public:
		struct CompositeField
		{
			std::string fields;
			const char *table;

			template <typename T>
			CompositeField (const Expression::Field<T> &field)
				: table (nullptr)
			{
				Extract (field);
			}

			template <typename T, typename... Args>
			CompositeField (const Expression::Field<T> &field,
							const Args & ... args)
				: CompositeField (args...)
			{
				Extract (field);
			}

		private:
			template <typename T>
			void Extract (const Expression::Field<T> &field)
			{
				if (table != nullptr && table != field.prefixStr)
					throw std::runtime_error (NOT_SAME_TABLE);
				table = field.prefixStr;
				if (fields.empty ())
					fields = field.fieldName;
				else
					fields = field.fieldName + "," + fields;
			}
		};

		template <typename T>
		static inline Constraint Default (
			const Expression::Field<T> &field,
			const T &value)
		{
			std::ostringstream os;
			BOT_ORM_Impl::SerializeValue (os << " default ", value);
			return Constraint { field.fieldName, os.str () };
		}

		static inline Constraint Check (
			const Expression::Expr &expr)
		{
			return Constraint { "",
				"check (" + expr.ToString (false) + ")" };
		}

		template <typename T>
		static inline Constraint Unique (
			const Expression::Field<T> &field)
		{
			return Constraint { "",
				"unique (" + field.fieldName + ")" };
		}

		static inline Constraint Unique (
			const CompositeField &fields)
		{
			return Constraint { "",
				"unique (" + fields.fields + ")" };
		}

		template <typename T>
		static inline Constraint Reference (
			const Expression::Field<T> &field,
			const Expression::Field<T> &refered)
		{
			return Constraint { "",
				std::string ("foreign key (") + field.fieldName +
				") references " + refered.prefixStr +
				"(" + refered.fieldName + ")" };
		}

		static inline Constraint Reference (
			const CompositeField &field,
			const CompositeField &refered)
		{
			return Constraint { "",
				std::string ("foreign key (") + field.fields +
				") references " + refered.table +
				"(" + refered.fields + ")" };
		}
	};

	template <typename T>
	class Queryable;
}

namespace BOT_ORM_Impl
{
	// Why Remove QueryableHelper from Query?
	// Query is a template but the Helper can be shared...

	class QueryableHelper
	{
		template <typename T>
		using Nullable = BOT_ORM::Nullable<T>;

		template <typename T>
		using Selectable = BOT_ORM::Expression::Selectable<T>;

	protected:
		template <typename T>
		friend class BOT_ORM::Queryable;

		// To Nullable
		// Get Nullable Value Wrappers for non-nullable Types
		template <typename T>
		static inline auto FieldToNullable (const T &val)
		{ return Nullable<T> (val); }

		template <typename T>
		static inline auto FieldToNullable (const Nullable<T> &val)
		{ return val; }

		template <typename TupleType, size_t N>
		struct TupleHelper
		{
			// Tuple Nullable Cater
			static inline auto ToNullable (const TupleType &tuple)
			{
				return std::tuple_cat (
					TupleHelper<TupleType, N - 1>::ToNullable (tuple),
					std::make_tuple (FieldToNullable (std::get<N - 1> (tuple)))
				);
			}

			// Tuple Visitor
			template <typename Fn>
			static inline void Visit (TupleType &tuple, Fn fn)
			{
				TupleHelper<TupleType, N - 1>::Visit (tuple, fn);
				fn (std::get<N - 1> (tuple));
			}
		};

		template <typename TupleType>
		struct TupleHelper <TupleType, 1>
		{
			static inline auto ToNullable (const TupleType &tuple)
			{
				return std::make_tuple (
					FieldToNullable (std::get<0> (tuple)));
			}

			template <typename Fn>
			static inline void Visit (TupleType &tuple, Fn fn)
			{
				fn (std::get<0> (tuple));
			}
		};

		// Flaten Arguments into Tuple
		template <typename C>
		static inline auto JoinToTuple (const C &arg)
		{
			// Injected friend

			using TupleType = decltype (arg.__Tuple ());
			constexpr size_t size = std::tuple_size<TupleType>::value;

			return TupleHelper<TupleType, size>::ToNullable (
				arg.__Tuple ());
		}
		template <typename... Args>
		static inline auto JoinToTuple (const std::tuple<Args...>& t)
		{
			// TupleHelper::ToNullable is not necessary
			return t;
		}
		template <typename Arg, typename... Args>
		static inline auto JoinToTuple (const Arg &arg,
										const Args & ... args)
		{
			return std::tuple_cat (JoinToTuple (arg),
								   JoinToTuple (args...));
		}

		// Return Select Target Tuple from Selectable List
		template <typename T>
		static inline auto SelectToTuple (const Selectable<T> &)
		{
			return std::make_tuple (Nullable<T> {});
		}

		template <typename T, typename... Args>
		static inline auto SelectToTuple (const Selectable<T> &arg,
										  const Args & ... args)
		{
			return std::tuple_cat (SelectToTuple (arg),
								   SelectToTuple (args...));
		}

		// Return Field Strings for GroupBy, OrderBy and Select
		template <typename T>
		static inline std::string FieldToSql (const Selectable<T> &op)
		{
			if (op.prefixStr)
				return op.prefixStr + ("." + op.fieldName);
			else
				return op.fieldName;
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
	protected:
		BOT_ORM_Impl::SQLConnector &_connector;
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

		Queryable (BOT_ORM_Impl::SQLConnector &connector,
				   QueryResult queryHelper,
				   std::string sqlFrom,
				   std::string sqlSelect = "select ",
				   std::string sqlTarget = "*",
				   std::string sqlWhere = "",
				   std::string sqlGroupBy = "",
				   std::string sqlHaving = "",
				   std::string sqlOrderBy = "",
				   std::string sqlLimit = "",
				   std::string sqlOffset = "")
			:
			_connector (connector),
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
						  const Expression::Expr &onExpr) const
		{
			return _NewJoinQuery (queryHelper2, onExpr, " join ");
		}
		template <typename C>
		inline auto LeftJoin (const C &queryHelper2,
							  const Expression::Expr &onExpr) const
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
			_connector.Execute (_sqlSelect + agg.fieldName +
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
		auto _NewQuery (std::string sqlTarget,
						std::string sqlFrom,
						std::tuple<Args...> &&newQueryHelper) const
		{
			return Queryable<std::tuple<Args...>>
			{
				_connector, newQueryHelper,
					std::move (sqlFrom),
					_sqlSelect, std::move (sqlTarget),
					_sqlWhere, _sqlGroupBy, _sqlHaving,
					_sqlOrderBy, _sqlLimit, _sqlOffset
			};
		}

		// Return a new Join Queryable Object
		template <typename C>
		inline auto _NewJoinQuery (const C &queryHelper2,
								   const Expression::Expr &onExpr,
								   std::string joinStr,
								   std::enable_if_t<
								   !BOT_ORM_Impl::HasInjected<C>::value>
								   * = nullptr) const
		{
			static_assert (BOT_ORM_Impl::HasInjected<C>::value, NO_ORMAP);
		}
		template <typename C>
		inline auto _NewJoinQuery (const C &queryHelper2,
								   const Expression::Expr &onExpr,
								   std::string joinStr,
								   std::enable_if_t<
								   BOT_ORM_Impl::HasInjected<C>::value>
								   * = nullptr) const
		{
			return _NewQuery (
				_sqlTarget,
				_sqlFrom + std::move (joinStr) + C::__TableName +
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
			_connector.Execute (_sqlSelect + _sqlTarget +
								_GetFromSql () + _GetLimit () + ";",
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
			_connector.Execute (_sqlSelect + _sqlTarget +
								_GetFromSql () + _GetLimit () + ";",
								[&] (int, char **argv, char **)
			{
				size_t index = 0;
				BOT_ORM_Impl::QueryableHelper::TupleHelper
					<QueryResult, sizeof... (Args)>
					::Visit (copy, [&argv, &index] (auto &val)
				{
					BOT_ORM_Impl::DeserializeValue (val, argv[index++]);
					return true;
				});
				out.push_back (copy);
			});
		}
	};

	// ORMapper

	class ORMapper
	{
	public:
		ORMapper (const std::string &connectionString)
			: _connector (connectionString)
		{
			_connector.Execute ("PRAGMA foreign_keys = ON;");
		}

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

		template <typename C, typename... Args>
		std::enable_if_t<!BOT_ORM_Impl::HasInjected<C>::value>
			CreateTbl (const C &entity, const Args & ... args)
		{
			static_assert (BOT_ORM_Impl::HasInjected<C>::value, NO_ORMAP);
		}
		template <typename C, typename... Args>
		std::enable_if_t<BOT_ORM_Impl::HasInjected<C>::value>
			CreateTbl (const C &entity, const Args & ... args)
		{
			const auto &fields = C::__FieldNames ();
			std::unordered_map<std::string, std::string> fieldFixes;
			size_t index = 0;

			entity.__Accept ([&fields, &fieldFixes, &index] (auto &val)
			{
				fieldFixes.emplace (fields[index++],
									std::string (" ") +
									_TypeString (val));
				return true;
			});
			fieldFixes[fields[0]] += " primary key";

			std::string tableFixes;
			_GetConstraints (tableFixes, fieldFixes, args...);

			std::string strFmt;
			for (const auto &field : fields)
				strFmt += field + fieldFixes[field] + ",";
			strFmt += std::move (tableFixes);
			strFmt.pop_back ();

			_connector.Execute ("create table " +
								std::string (C::__TableName) +
								"(" + strFmt + ");");
		}

		template <typename C>
		std::enable_if_t<!BOT_ORM_Impl::HasInjected<C>::value>
			DropTbl (const C &)
		{
			static_assert (BOT_ORM_Impl::HasInjected<C>::value, NO_ORMAP);
		}
		template <typename C>
		std::enable_if_t<BOT_ORM_Impl::HasInjected<C>::value>
			DropTbl (const C &)
		{
			_connector.Execute ("drop table " +
								std::string (C::__TableName) +
								";");
		}

		template <typename C>
		std::enable_if_t<!BOT_ORM_Impl::HasInjected<C>::value>
			Insert (const C &entity, bool withId = true)
		{
			static_assert (BOT_ORM_Impl::HasInjected<C>::value, NO_ORMAP);
		}
		template <typename C>
		std::enable_if_t<BOT_ORM_Impl::HasInjected<C>::value>
			Insert (const C &entity, bool withId = true)
		{
			std::ostringstream os;
			_GetInsert (os, entity, withId);
			_connector.Execute (os.str ());
		}

		template <typename In, typename C = typename In::value_type>
		std::enable_if_t<!BOT_ORM_Impl::HasInjected<C>::value>
			InsertRange (const In &entities, bool withId = true)
		{
			static_assert (
				BOT_ORM_Impl::HasInjected<C>::value, NO_ORMAP);
		}
		template <typename In, typename C = typename In::value_type>
		std::enable_if_t<BOT_ORM_Impl::HasInjected<C>::value>
			InsertRange (const In &entities, bool withId = true)
		{
			std::ostringstream os;
			for (const auto &entity : entities)
				_GetInsert (os, entity, withId);
			_connector.Execute (os.str ());
		}

		template <typename C>
		std::enable_if_t<!BOT_ORM_Impl::HasInjected<C>::value>
			Update (const C &entity)
		{
			static_assert (BOT_ORM_Impl::HasInjected<C>::value, NO_ORMAP);
		}
		template <typename C>
		std::enable_if_t<BOT_ORM_Impl::HasInjected<C>::value>
			Update (const C &entity)
		{
			std::ostringstream os;
			_GetUpdate (os, entity);
			_connector.Execute (os.str ());
		}

		template <typename In, typename C = typename In::value_type>
		std::enable_if_t<!BOT_ORM_Impl::HasInjected<C>::value>
			UpdateRange (const In &entities)
		{
			static_assert (BOT_ORM_Impl::HasInjected<C>::value, NO_ORMAP);
		}
		template <typename In, typename C = typename In::value_type>
		std::enable_if_t<BOT_ORM_Impl::HasInjected<C>::value>
			UpdateRange (const In &entities)
		{
			std::ostringstream os;
			for (const auto &entity : entities)
				_GetUpdate (os, entity);
			_connector.Execute (os.str ());
		}

		template <typename C>
		std::enable_if_t<!BOT_ORM_Impl::HasInjected<C>::value>
			Update (const C &,
					const Expression::SetExpr &setExpr,
					const Expression::Expr &whereExpr)
		{
			static_assert (BOT_ORM_Impl::HasInjected<C>::value, NO_ORMAP);
		}
		template <typename C>
		std::enable_if_t<BOT_ORM_Impl::HasInjected<C>::value>
			Update (const C &,
					const Expression::SetExpr &setExpr,
					const Expression::Expr &whereExpr)
		{
			_connector.Execute ("update " +
								std::string (C::__TableName) +
								" set " + setExpr.ToString () +
								" where " +
								whereExpr.ToString (false) + ";");
		}

		template <typename C>
		std::enable_if_t<!BOT_ORM_Impl::HasInjected<C>::value>
			Delete (const C &entity)
		{
			static_assert (BOT_ORM_Impl::HasInjected<C>::value, NO_ORMAP);
		}
		template <typename C>
		std::enable_if_t<BOT_ORM_Impl::HasInjected<C>::value>
			Delete (const C &entity)
		{
			std::ostringstream os;
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
		std::enable_if_t<!BOT_ORM_Impl::HasInjected<C>::value>
			Delete (const C &,
					const Expression::Expr &whereExpr)
		{
			static_assert (BOT_ORM_Impl::HasInjected<C>::value, NO_ORMAP);
		}
		template <typename C>
		std::enable_if_t<BOT_ORM_Impl::HasInjected<C>::value>
			Delete (const C &,
					const Expression::Expr &whereExpr)
		{
			_connector.Execute ("delete from " +
								std::string (C::__TableName) +
								" where " +
								whereExpr.ToString (false) + ";");
		}

		template <typename C>
		std::enable_if_t<!BOT_ORM_Impl::HasInjected<C>::value, Queryable<C>>
			Query (C queryHelper)
		{
			static_assert (BOT_ORM_Impl::HasInjected<C>::value, NO_ORMAP);
		}
		template <typename C>
		std::enable_if_t<BOT_ORM_Impl::HasInjected<C>::value, Queryable<C>>
			Query (C queryHelper)
		{
			return Queryable<C> { _connector,
				std::move (queryHelper),
				std::string (" from ") + C::__TableName };
		}

	protected:
		BOT_ORM_Impl::SQLConnector _connector;

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
				? "integer not null"
				: (std::is_floating_point<T>::value)
				? "real not null"
				: (std::is_same<std::remove_cv_t<T>, std::string>::value)
				? "text not null"
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
				? "integer"
				: (std::is_floating_point<T>::value)
				? "real"
				: (std::is_same<std::remove_cv_t<T>, std::string>::value)
				? "text"
				: nullptr;

			static_assert (typeStr != nullptr, BAD_TYPE);
			return typeStr;
		}

		void _GetConstraints (
			std::string &,
			std::unordered_map<std::string, std::string> &)
		{}

		template <typename... Args>
		void _GetConstraints (
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
		void _GetInsert (std::ostream &os, const C &entity, bool withId)
		{
			os << "insert into " << std::string (C::__TableName) << "(";

			std::ostringstream osVal;
			size_t index = 0;
			const auto &fieldNames = C::__FieldNames ();

			entity.__Accept ([&osVal, &os,
							 &index, &fieldNames, withId] (auto &val)
			{
				if ((index != 0 || withId) &&
					BOT_ORM_Impl::SerializeValue (osVal, val))
				{
					os << fieldNames[index] << ',';
					osVal << ',';
				}
				index++;
				return true;
			});

			os.seekp (os.tellp () - std::streamoff (1));
			osVal.seekp (osVal.tellp () - std::streamoff (1));

			osVal << ");";  // Enable Seekp...
			os << ") values (" << osVal.str ();
		}

		template <typename C>
		void _GetUpdate (std::ostream &os, const C &entity) const
		{
			os << "update " << C::__TableName << " set ";

			const auto &fieldNames = C::__FieldNames ();
			size_t index = 0;
			std::ostringstream osKey;

			entity.__Accept ([&os, &osKey, &index, &fieldNames] (auto &val)
			{
				if (index == 0)
				{
					osKey << fieldNames[index++] << "=";
					if (!BOT_ORM_Impl::SerializeValue (osKey, val))
						osKey << "null";
				}
				else
				{
					os << fieldNames[index++] << "=";
					if (!BOT_ORM_Impl::SerializeValue (os, val))
						os << "null";
					os << ',';
				}
				return true;
			});

			os.seekp (os.tellp () - std::streamoff (1));
			os << " where " << osKey.str () << ";";
		}
	};

	// Field Extractor

	class FieldExtractor
	{
		using pair_type = std::pair<std::string, const char *>;

		template <typename C>
		std::enable_if_t<!BOT_ORM_Impl::HasInjected<C>::value>
			Extract (const C &helper)
		{
			static_assert (BOT_ORM_Impl::HasInjected<C>::value, NO_ORMAP);
		}
		template <typename C>
		std::enable_if_t<BOT_ORM_Impl::HasInjected<C>::value>
			Extract (const C &helper)
		{
			// Why Place these local vars here:
			// Walk around gcc/clang 'undefined reference' HELL...
			const auto &fieldNames = C::__FieldNames ();
			constexpr auto tableName = C::__TableName;

			size_t index = 0;
			helper.__Accept (
				[this, &index, &fieldNames, &tableName] (auto &val)
			{
				_map.emplace ((const void *) &val, pair_type {
					fieldNames[index++], tableName });
				return true;
			});
		}

	public:
		template <typename Arg>
		FieldExtractor (const Arg &arg) { Extract (arg); }

		template <typename Arg, typename... Args>
		FieldExtractor (const Arg &arg, const Args & ... args)
			: FieldExtractor (args...) { Extract (arg); }

		template <typename T>
		inline Expression::Field<T> operator () (
			const T &field) const
		{
			const auto &result = Get (field);
			return Expression::Field<T> {
				std::move (result.first), result.second };
		}

		template <typename T>
		inline Expression::NullableField<T> operator () (
			const Nullable<T> &field) const
		{
			const auto &result = Get (field);
			return Expression::NullableField<T> {
				std::move (result.first), result.second };
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