
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
template <typename VISITOR, typename FN>                  \
void __Accept (const VISITOR &visitor, FN fn)             \
{                                                         \
    visitor.Visit (fn, __VA_ARGS__);                      \
}                                                         \
template <typename VISITOR, typename FN>                  \
void __Accept (const VISITOR &visitor, FN fn) const       \
{                                                         \
    visitor.Visit (fn, __VA_ARGS__);                      \
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
	class Nullable final
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
		T &property,
		const BOT_ORM::Nullable<std::string> &value)
	{
		if (value == nullptr)
			throw std::runtime_error ("Null to Deserialze");
		std::stringstream ostr (value.Value ());
		ostr >> property;
	}

	template <>
	inline void DeserializeValue <std::string> (
		std::string &property,
		const BOT_ORM::Nullable<std::string> &value)
	{
		if (value == nullptr)
			throw std::runtime_error ("Null to Deserialze");
		property = std::move (value.Value ());
	}

	template <typename T>
	inline void DeserializeValue (
		BOT_ORM::Nullable<T> &property,
		const BOT_ORM::Nullable<std::string> &value)
	{
		if (value == nullptr)
			property = nullptr;
		else
		{
			T val;
			DeserializeValue (val, value);
			property = val;
		}
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

	// Fn Visitor
	class FnVisitor
	{
	public:
		template <typename Fn, typename... Args>
		inline void Visit (Fn fn, Args & ... args) const
		{
			_Visit (fn, args...);
		}

	protected:
		template <typename Fn, typename T, typename... Args>
		inline void _Visit (Fn fn, T &property, Args & ... args) const
		{
			if (_Visit (fn, property))
				_Visit (fn, args...);
		}

		template <typename Fn, typename T>
		inline bool _Visit (Fn fn, T &property) const
		{
			return fn (property);
		}
	};

	// Using a _SizeT to specify the Index :-), Cool
	template <size_t> struct _SizeT {};

	// Tuple Visitor
	// http://stackoverflow.com/questions/18155533/how-to-iterate-through-stdtuple

	template <typename TupleType, typename ActionType>
	inline void TupleVisitor (TupleType &tuple, ActionType action)
	{
		TupleVisitor_Impl (tuple, action,
						   _SizeT<std::tuple_size<TupleType>::value> ());
	}

	template <typename TupleType, typename ActionType>
	inline void TupleVisitor_Impl (TupleType &tuple, ActionType action,
								   _SizeT<0>) {}

	template <typename TupleType, typename ActionType, size_t N>
	inline void TupleVisitor_Impl (TupleType &tuple, ActionType action,
								   _SizeT<N>)
	{
		TupleVisitor_Impl (tuple, action, _SizeT<N - 1> ());
		action (std::get<N - 1> (tuple));
	}

	// Helper - Extract Field Names
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

			entity.__Accept (BOT_ORM_Impl::FnVisitor (),
							 [&strTypes, &index] (auto &val)
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

			entity.__Accept (BOT_ORM_Impl::FnVisitor (),
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
				entity.__Accept (BOT_ORM_Impl::FnVisitor (),
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

			entity.__Accept (BOT_ORM_Impl::FnVisitor (),
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
					BOT_ORM_Impl::FnVisitor (),
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
				os << " where " + osKey.str () + ";";
			}
			_connector.Execute (os.str ());
		}

		template <typename C>
		void Delete (const C &entity)
		{
			std::stringstream os;
			os << "where ";

			entity.__Accept (BOT_ORM_Impl::FnVisitor (),
							 [&os] (auto &val)
			{
				os << C::__FieldNames ()[0] << "=";
				BOT_ORM_Impl::SerializeValue (os, val);
				return false;
			});

			_connector.Execute ("delete from " +
								std::string (C::__TableName) +
								" " + os.str () + ";");
		}

		// Basic Utility: Expr, SetExpr

		struct Field
		{
			std::string fieldName;
			const char *tableName;
		};

		class Expr
		{
		public:
			Expr (const Field &field, std::string op_val)
				: exprs
			{ {
				field.fieldName + std::move (op_val),
				field.tableName
			} }
			{}

			Expr (const Field &field1,
				  std::string op,
				  const Field &field2)
				: exprs
			{
				{ field1.fieldName, field1.tableName },
				{ std::move (op), nullptr},
				{field2.fieldName, field2.tableName }
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
				ret.exprs.splice (ret.exprs.cend (), std::move (rigthExprs));
				ret.exprs.emplace_back (")", nullptr);
				return std::move (ret);
			}
		};

		struct SetExpr
		{
			SetExpr (const Field &field,
					 std::string op_val)
				: expr { field.fieldName + op_val }
			{}

			const std::string &ToString () const
			{ return expr; }

		protected:
			std::string expr;
		};

		// Basic Utility: Field, NullableField

		template <typename T>
		struct NormalField : public Field
		{
			NormalField (std::string fieldName,
						 const char *tableName)
				: Field
			{
				std::move (fieldName),
				std::move (tableName)
			}
			{}

			inline SetExpr operator = (T value)
			{
				std::ostringstream os;
				BOT_ORM_Impl::SerializeValue (os << "=",
											  value);
				return SetExpr { *this, os.str () };
			}

		private:
			// Invalid Expr: Field is Not Null
			inline SetExpr operator = (nullptr_t)
			{ return SetExpr {}; }
			inline Expr operator == (nullptr_t)
			{ return Expr {}; }
			inline Expr operator != (nullptr_t)
			{ return Expr {}; }
		};

		template <typename T>
		struct NullableField : public NormalField<T>
		{
			NullableField (std::string fieldName,
						   const char *tableName)
				: NormalField<T>
			{
				std::move (fieldName),
				std::move (tableName)
			}
			{}

			inline SetExpr operator = (T value)
			{
				std::ostringstream os;
				BOT_ORM_Impl::SerializeValue (os << "=",
											  value);
				return SetExpr { *this, os.str () };
			}

			inline SetExpr operator = (nullptr_t)
			{ return SetExpr { *this, "=null" }; }
			inline Expr operator == (nullptr_t)
			{ return Expr { *this, " is null" }; }
			inline Expr operator != (nullptr_t)
			{ return Expr { *this, " is not null" }; }
		};

		// Basic Utility: FieldExtractor

		template <typename C>
		class FieldExtractor
		{
			const C &_queryHelper;

			template <typename T>
			std::string _FieldName (const T &property)
			{
				size_t index = 0;
				_queryHelper.__Accept (BOT_ORM_Impl::FnVisitor (),
									   [&property, &index] (auto &val)
				{
					if ((const void *) &property == (const void *) &val)
						return false;
					index++;
					return true;
				});

				const auto &fieldNames = C::__FieldNames ();
				if (index == fieldNames.size ())
					throw std::runtime_error ("No such Field in the Table");
				return fieldNames[index];
			}

		public:
			FieldExtractor (const C &queryHelper)
				: _queryHelper (queryHelper) {}

			template <typename T>
			inline NormalField<T> operator () (const T &property)
			{ return NormalField<T> { _FieldName (property), C::__TableName }; }

			template <typename T>
			inline NullableField<T> operator () (const Nullable<T> &property)
			{ return NullableField<T> { _FieldName (property), C::__TableName }; }
		};

		// Update & Delete By Condition

		template <typename C>
		inline void Delete (const C &,
							const Expr &expr)
		{
			_connector.Execute ("delete from " +
								std::string (C::__TableName) +
								" where " + expr.ToString (false) + ";");
		}

		template <typename C, typename... Args>
		void Update (const C &,
					 const Expr &expr,
					 const Args & ... setExprs)
		{
			std::stringstream setSql;
			setSql << "set ";
			auto count = sizeof... (Args);
			BOT_ORM_Impl::FnVisitor {}.Visit (
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

		// Query Utility: AggregateFunc

		template <typename T>
		struct AggregateFunc
		{
			std::string function;
		};

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
					   std::string sqlOrderBy = "",
					   std::string sqlLimit = "",
					   std::string sqlOffset = "") :
				_pMapper (pMapper),
				_queryHelper (std::move (queryHelper)),
				_sqlFrom (std::move (queryFrom)),
				_sqlTarget (std::move (queryTarget)),
				_sqlWhere (std::move (sqlWhere)),
				_sqlOrderBy (std::move (sqlOrderBy)),
				_sqlLimit (std::move (sqlLimit)),
				_sqlOffset (std::move (sqlOffset))
			{}

			// Result Type
			typedef typename QueryResult result_type;

			// Where
			inline Queryable &Where (const Expr &expr)
			{
				_sqlWhere = " where (" + expr.ToString (true) + ")";
				return *this;
			}

			// Order By
			inline Queryable &OrderBy (const Field &field)
			{
				_sqlOrderBy = " order by " +
					(field.tableName + ("." + field.fieldName));
				return *this;
			}
			inline Queryable &OrderByDescending (const Field &field)
			{
				_sqlOrderBy = " order by " +
					(field.tableName + ("." + field.fieldName)) + " desc";
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
				_sqlOffset = " offset " + std::to_string (count);
				return *this;
			}

			// Join
			template <typename C>
			auto Join (const C &queryHelper2,
					   const Expr &onExpr)
			{
				return _Transform (
					_sqlTarget,
					_sqlFrom + " join " +
					std::string (C::__TableName) +
					" on " + onExpr.ToString (true),
					_ToTuple (_queryHelper, queryHelper2));
			}

			template <typename C>
			auto LeftJoin (const C &queryHelper2,
						   const Expr &onExpr)
			{
				return _Transform (
					_sqlTarget,
					_sqlFrom + " left join " +
					std::string (C::__TableName) +
					" on " + onExpr.ToString (true),
					_ToTuple (_queryHelper, queryHelper2));
			}

			template <typename T>
			auto GroupBy (const NormalField<T> &field)
			{
				// Todo
				// Having
			}

			// Select
			template <typename... Args>
			auto Select (const Args & ... args)
			{
				return _Transform (
					_GetTargetSql (args...),
					_sqlFrom,
					_SelectToTuple (args...)
				);
			}

			// Aggregate
			template <typename T>
			T Aggregate (const AggregateFunc<T> &agg)
			{
				T ret;
				auto retStr = _pMapper->_Select (agg.function,
												 _GetFromSql ());
				BOT_ORM_Impl::DeserializeValue (ret, retStr);
				return std::move (ret);
			}

			// Retrieve Select Result
			std::vector<QueryResult> ToVector ()
			{
				std::vector<QueryResult> ret;
				_pMapper->_Select (_queryHelper, ret, _sqlTarget, _GetFromSql ());
				return std::move (ret);
			}

			std::list<QueryResult> ToList ()
			{
				std::list<QueryResult> ret;
				_pMapper->_Select (_queryHelper, ret, _sqlTarget, _GetFromSql ());
				return std::move (ret);
			}

		protected:
			ORMapper *_pMapper;
			QueryResult _queryHelper;

			std::string _sqlFrom;
			std::string _sqlTarget;

			std::string _sqlWhere;
			std::string _sqlOrderBy;
			std::string _sqlLimit, _sqlOffset;

			// Return "from ..." for Query
			inline std::string _GetFromSql () const
			{
				return _sqlFrom + _sqlWhere +
					_sqlOrderBy + _sqlLimit + _sqlOffset;
			}

			// Return a new Queryable Object
			template <typename... Args>
			auto _Transform (std::string target,
							 std::string from,
							 std::tuple<Args...> newQueryHelper)
			{
				return Queryable<decltype (newQueryHelper)>
				{
					_pMapper,
						std::move (newQueryHelper),
						"(" + std::move (from) + ")",
						std::move (target),
						_sqlWhere, _sqlOrderBy,
						_sqlLimit, _sqlOffset
				};
			}

			// Return Select Target String
			template <typename T>
			static inline std::string _GetTargetSql (const NormalField<T> &field)
			{
				return field.tableName + ("." + field.fieldName);
			}
			template <typename T>
			static inline std::string _GetTargetSql (const NullableField<T> &field)
			{
				return field.tableName + ("." + field.fieldName);
			}
			template <typename T>
			static inline std::string _GetTargetSql (const AggregateFunc<T> &agg)
			{
				return agg.function;
			}
			template <typename Arg, typename... Args>
			static inline std::string _GetTargetSql (const Arg &arg,
													 const Args & ... args)
			{
				return _GetTargetSql (arg) + "," + _GetTargetSql (args...);
			}

			// Return Select Target Tuple
			template <typename T>
			static inline auto _SelectToTuple (const NormalField<T> &field)
			{
				return std::make_tuple (Nullable<T> {});
			}
			template <typename T>
			static inline auto _SelectToTuple (const NullableField<T> &field)
			{
				return std::make_tuple (Nullable<T> {});
			}
			template <typename T>
			static inline auto _SelectToTuple (const AggregateFunc<T> &agg)
			{
				return std::make_tuple (Nullable<T> {});
			}
			template <typename Arg, typename... Args>
			static inline auto _SelectToTuple (const Arg &arg,
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
			template <typename TupleType>
			static inline auto _ToNullableTuple (TupleType &tuple,
												 BOT_ORM_Impl::_SizeT<1>)
			{
				return std::make_tuple (_ToNullable (std::get<0> (tuple)));
			}
			template <typename TupleType, size_t N>
			static inline auto _ToNullableTuple (TupleType &tuple,
												 BOT_ORM_Impl::_SizeT<N>)
			{
				return std::tuple_cat (
					_ToNullableTuple (tuple, BOT_ORM_Impl::_SizeT<N - 1> {}),
					std::make_tuple (_ToNullable (std::get<N - 1> (tuple)))
				);
			}

			// Flaten Arguments into Tuple
			template <typename C>
			static inline auto _ToTuple (const C &arg)
			{
				return _ToNullableTuple (
					arg.__Tuple (),
					BOT_ORM_Impl::_SizeT<
					std::tuple_size<decltype (arg.__Tuple ())
					>::value> {}
				);
			}
			template <typename... Args>
			static inline auto _ToTuple (const std::tuple<Args...>& t)
			{
				// _ToNullableTuple is not necessary
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
		Queryable<C> Query (C queryHelper)
		{
			static_assert (std::is_copy_constructible<C>::value,
						   "It must be Copy Constructible");

			return Queryable<C> (this, std::move (queryHelper),
								 std::string (C::__TableName));
		}

	private:
		BOT_ORM_Impl::SQLConnector _connector;

		template <typename C, typename Out>
		void _Select (C copy, Out &out,
					  const std::string &sqlTarget,
					  const std::string &sqlFrom)
		{
			_connector.Execute ("select " + sqlTarget +
								" from " + sqlFrom + ";",
								[&] (int, char **argv, char **)
			{
				size_t index = 0;
				copy.__Accept (
					BOT_ORM_Impl::FnVisitor (),
					[&argv, &index] (auto &val)
				{
					if (argv[index])
						BOT_ORM_Impl::DeserializeValue (
							val, BOT_ORM::Nullable<std::string> (argv[index]));
					else
						BOT_ORM_Impl::DeserializeValue (
							val, BOT_ORM::Nullable<std::string> (nullptr));
					index++;
					return true;
				});
				out.push_back (copy);
			});
		}

		template <typename Out, typename... Args>
		void _Select (std::tuple<Args...> copy, Out &out,
					  const std::string &sqlTarget,
					  const std::string &sqlFrom)
		{
			_connector.Execute ("select " + sqlTarget +
								" from " + sqlFrom + ";",
								[&] (int argc, char **argv, char **)
			{
				size_t index = 0;
				BOT_ORM_Impl::TupleVisitor (
					copy, [&argv, &index] (auto &val)
				{
					if (argv[index])
						BOT_ORM_Impl::DeserializeValue (
							val, BOT_ORM::Nullable<std::string> (argv[index]));
					else
						BOT_ORM_Impl::DeserializeValue (
							val, BOT_ORM::Nullable<std::string> (nullptr));
					index++;
					return true;
				});
				out.push_back (copy);
			});
		}

		std::string _Select (const std::string &sqlTarget,
							 const std::string &sqlFrom)
		{
			std::string ret;
			_connector.Execute ("select " + sqlTarget +
								" from " + sqlFrom + ";",
								[&] (int argc, char **argv, char **)
			{
				if (argv[0]) ret = argv[0];
			});

			if (ret.empty ())
				throw std::runtime_error ("SQL error: Return null;"
										  " Maybe such column is Empty...");
			return ret;
		}
	};
}

namespace BOT_ORM_Impl
{
	template <typename T>
	BOT_ORM::ORMapper::Expr ToExpr (
		const BOT_ORM::ORMapper::NormalField<T> &field,
		const char *op, const T &value)
	{
		std::ostringstream os;
		SerializeValue (os << op, value);
		return BOT_ORM::ORMapper::Expr { field, os.str () };
	}
}

namespace BOT_ORM
{
	namespace FieldHelper
	{
		using Expr = ORMapper::Expr;
		using Field_t = ORMapper::Field;

		inline Expr operator == (const Field_t &op1, const Field_t &op2)
		{ return Expr { op1 , "=", op2 }; }
		inline Expr operator != (const Field_t &op1, const Field_t &op2)
		{ return Expr { op1, "!=", op2 }; }

		inline Expr operator > (const Field_t &op1, const Field_t &op2)
		{ return Expr { op1 , ">", op2 }; }
		inline Expr operator >= (const Field_t &op1, const Field_t &op2)
		{ return Expr { op1, ">=", op2 }; }

		inline Expr operator < (const Field_t &op1, const Field_t &op2)
		{ return Expr { op1 , "<", op2 }; }
		inline Expr operator <= (const Field_t &op1, const Field_t &op2)
		{ return Expr { op1, "<=", op2 }; }

		template <typename T>
		inline Expr operator == (const ORMapper::NormalField<T> &field,
								 T value)
		{
			return BOT_ORM_Impl::ToExpr (field, "=", std::move (value));
		}
		template <typename T>
		inline Expr operator != (const ORMapper::NormalField<T> &field,
								 T value)
		{
			return BOT_ORM_Impl::ToExpr (field, "!=", std::move (value));
		}

		template <typename T>
		inline Expr operator > (const ORMapper::NormalField<T> &field,
								T value)
		{
			return BOT_ORM_Impl::ToExpr (field, ">", std::move (value));
		}
		template <typename T>
		inline Expr operator >= (const ORMapper::NormalField<T> &field,
								 T value)
		{
			return BOT_ORM_Impl::ToExpr (field, ">=", std::move (value));
		}

		template <typename T>
		inline Expr operator < (const ORMapper::NormalField<T> &field,
								T value)
		{
			return BOT_ORM_Impl::ToExpr (field, "<", std::move (value));
		}
		template <typename T>
		inline Expr operator <= (const ORMapper::NormalField<T> &field,
								 T value)
		{
			return BOT_ORM_Impl::ToExpr (field, "<=", std::move (value));
		}

		inline Expr operator & (
			const ORMapper::NormalField<std::string> &field,
			std::string likePattern)
		{
			return BOT_ORM_Impl::ToExpr (field, " like ",
										 std::move (likePattern));
		}
		inline Expr operator | (
			const ORMapper::NormalField<std::string> &field,
			std::string likePattern)
		{
			return BOT_ORM_Impl::ToExpr (field, " not like ",
										 std::move (likePattern));
		}

		template <typename C>
		inline ORMapper::FieldExtractor<C> Field (const C &queryHelper)
		{
			return ORMapper::FieldExtractor<C> { queryHelper };
		}
	}

	namespace AggregateHelper
	{
		inline auto Count ()
		{
			return ORMapper::AggregateFunc<size_t> { "count (*)" };
		}
		template <typename T>
		inline auto Count (const ORMapper::NormalField<T> &field)
		{
			return ORMapper::AggregateFunc<size_t> { "count (" +
				(field.tableName + ("." + field.fieldName)) + ")" };
		}
		template <typename T>
		inline auto Sum (const ORMapper::NormalField<T> &field)
		{
			return ORMapper::AggregateFunc<T> { "sum (" +
				(field.tableName + ("." + field.fieldName)) + ")" };
		}
		template <typename T>
		inline auto Avg (const ORMapper::NormalField<T> &field)
		{
			return ORMapper::AggregateFunc<T> { "avg (" +
				(field.tableName + ("." + field.fieldName)) + ")" };
		}
		template <typename T>
		inline auto Max (const ORMapper::NormalField<T> &field)
		{
			return ORMapper::AggregateFunc<T> { "max (" +
				(field.tableName + ("." + field.fieldName)) + ")" };
		}
		template <typename T>
		inline auto Min (const ORMapper::NormalField<T> &field)
		{
			return ORMapper::AggregateFunc<T> { "min (" +
				(field.tableName + ("." + field.fieldName)) + ")" };
		}
	}
}

#endif // !BOT_ORM_H