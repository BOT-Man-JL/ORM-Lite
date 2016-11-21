
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
#include <cstddef>

#include "sqlite3.h"

// Public Macro

#define ORMAP(_MY_CLASS_, ...)                            \
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
static const std::vector<std::string> &__FieldNames ()    \
{                                                         \
    static const std::vector<std::string> _fieldNames {   \
        BOT_ORM_Impl::ExtractFieldName (#__VA_ARGS__) };  \
    return _fieldNames;                                   \
}                                                         \
constexpr static const char *__TableName =  #_MY_CLASS_;  \

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
	template < size_t > struct _SizeT {};

	// Tuple Visitor
	// http://stackoverflow.com/questions/18155533/how-to-iterate-through-stdtuple

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
	namespace Helper
	{
		enum class JoinTypes
		{
			Inner, Left, Right, Full
		};
	}

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
		void Insert (const C &entity)
		{
			std::ostringstream os;
			size_t index = 0;
			auto fieldCount = C::__FieldNames ().size ();

			entity.__Accept (BOT_ORM_Impl::FnVisitor (),
							 [&os, &index, fieldCount] (auto &val)
			{
				if (++index != fieldCount)
					BOT_ORM_Impl::SerializeValue (os, val) << ',';
				else
					BOT_ORM_Impl::SerializeValue (os, val);
				return true;
			});

			_connector.Execute ("insert into " +
								std::string (C::__TableName) +
								" values (" + os.str () + ");");
		}

		template <typename In>
		void InsertRange (const In &entities)
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
								 [&os, &index, fieldCount] (auto &val)
				{
					if (++index != fieldCount)
						BOT_ORM_Impl::SerializeValue (os, val) << ',';
					else
						BOT_ORM_Impl::SerializeValue (os, val);
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

		struct Expr
		{
			std::string expr;

			inline Expr operator && (const Expr &right) const
			{ return And_Or (right, " and "); }
			inline Expr operator || (const Expr &right) const
			{ return And_Or (right, " or "); }

		protected:
			Expr And_Or (const Expr &right, std::string logOp) const
			{
				auto ret = *this;
				ret.expr = "(" + ret.expr + std::move (logOp)
					+ right.expr + ")";
				return std::move (ret);
			}
		};

		struct SetExpr
		{
			std::string expr;
		};

		// Basic Utility: Field, NullableField

		template <typename T>
		struct Field
		{
			std::string fieldName;

			inline SetExpr operator = (T value)
			{
				std::ostringstream os;
				BOT_ORM_Impl::SerializeValue (os << fieldName << "=",
											  value);
				return SetExpr { os.str () };
			}

			// With Value

			inline Expr operator == (T value)
			{ return ToExpr ("=", std::move (value)); }
			inline Expr operator != (T value)
			{ return ToExpr ("!=", std::move (value)); }

			inline Expr operator > (T value)
			{ return ToExpr (">", std::move (value)); }
			inline Expr operator >= (T value)
			{ return ToExpr (">=", std::move (value)); }

			inline Expr operator < (T value)
			{ return ToExpr ("<", std::move (value)); }
			inline Expr operator <= (T value)
			{ return ToExpr ("<=", std::move (value)); }

			// With Field

			inline Expr operator == (const Field &op)
			{ return Expr { fieldName + "=" + op.fieldName }; }
			inline Expr operator != (const Field &op)
			{ return Expr { fieldName + "!=" + op.fieldName }; }

			inline Expr operator > (const Field &op)
			{ return Expr { fieldName + ">" + op.fieldName }; }
			inline Expr operator >= (const Field &op)
			{ return Expr { fieldName + ">=" + op.fieldName }; }

			inline Expr operator < (const Field &op)
			{ return Expr { fieldName + "<" + op.fieldName }; }
			inline Expr operator <= (const Field &op)
			{ return Expr { fieldName + "<=" + op.fieldName }; }

		protected:
			Expr ToExpr (std::string op, const T &value)
			{
				std::ostringstream os;
				BOT_ORM_Impl::SerializeValue (os << fieldName << op,
											  value);
				return Expr { os.str () };
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
		struct NullableField : public Field<T>
		{
			NullableField (std::string fieldName)
				: Field<T> { std::move (fieldName) } {}

			inline SetExpr operator = (nullptr_t)
			{ return SetExpr { Field<T>::fieldName + "=null" }; }
			inline Expr operator == (nullptr_t)
			{ return Expr { Field<T>::fieldName + " is null" }; }
			inline Expr operator != (nullptr_t)
			{ return Expr { Field<T>::fieldName + " is not null" }; }

			inline SetExpr operator = (T value)
			{
				std::ostringstream os;
				BOT_ORM_Impl::SerializeValue (
					os << Field<T>::fieldName << "=",
					value);
				return SetExpr { os.str () };
			}

			// With Value

			inline Expr operator == (T value)
			{ return Field<T>::ToExpr ("=", std::move (value)); }
			inline Expr operator != (T value)
			{ return Field<T>::ToExpr ("!=", std::move (value)); }

			inline Expr operator > (T value)
			{ return Field<T>::ToExpr (">", std::move (value)); }
			inline Expr operator >= (T value)
			{ return Field<T>::ToExpr (">=", std::move (value)); }

			inline Expr operator < (T value)
			{ return Field<T>::ToExpr ("<", std::move (value)); }
			inline Expr operator <= (T value)
			{ return Field<T>::ToExpr ("<=", std::move (value)); }

			// With Field

			inline Expr operator == (const Field<T> &op)
			{ return Expr { Field<T>::fieldName + "=" + op.fieldName }; }
			inline Expr operator != (const Field<T> &op)
			{ return Expr { Field<T>::fieldName + "!=" + op.fieldName }; }

			inline Expr operator > (const Field<T> &op)
			{ return Expr { Field<T>::fieldName + ">" + op.fieldName }; }
			inline Expr operator >= (const Field<T> &op)
			{ return Expr { Field<T>::fieldName + ">=" + op.fieldName }; }

			inline Expr operator < (const Field<T> &op)
			{ return Expr { Field<T>::fieldName + "<" + op.fieldName }; }
			inline Expr operator <= (const Field<T> &op)
			{ return Expr { Field<T>::fieldName + "<=" + op.fieldName }; }
		};

		// Basic Utility: FieldExtractor

		template <typename C>
		class FieldExtractor
		{
			const C &_queryHelper;

			template <typename T>
			std::string FieldName (const T &property)
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
				//return C::__TableName + ("." + fieldNames[index]);
			}

		public:
			FieldExtractor (const C &queryHelper)
				: _queryHelper (queryHelper) {}

			template <typename T>
			inline Field<T> operator () (const T &property)
			{ return Field<T> { FieldName (property) }; }

			template <typename T>
			inline NullableField<T> operator () (const Nullable<T> &property)
			{ return NullableField<T> { FieldName (property) }; }
		};

		// Update & Delete By Condition

		template <typename C>
		inline void Delete (const C &,
							const Expr &expr)
		{
			_connector.Execute ("delete from " +
								std::string (C::__TableName) +
								" where " + expr.expr + ";");
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
				setSql << setExpr.expr;
				if (--count != 0) setSql << ",";
				return true;
			}, setExprs...);
			_connector.Execute ("update " +
								std::string (C::__TableName) +
								" " + setSql.str () +
								" where " + expr.expr + ";");
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
			Queryable (const QueryResult &queryHelper,
					   std::string querySource,
					   ORMapper *pMapper) :
				_queryHelper (queryHelper),
				_querySource (std::move (querySource)),
				_pMapper (pMapper)
			{}

			// Where
			inline Queryable &Where (const Expr &expr)
			{
				_sqlWhere = " where (" + expr.expr + ")";
				return *this;
			}

			// Order By
			template <typename T>
			inline Queryable &OrderBy (const Field<T> &field)
			{
				_sqlOrderBy = " order by " + field.fieldName;
				return *this;
			}
			template <typename T>
			inline Queryable &OrderByDescending (const Field<T> &field)
			{
				_sqlOrderBy = " order by " + field.fieldName + " desc";
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

			// Todo

			// Join
			//template <typename C>
			//auto Join (const C &queryHelper2,
			//		   Helper::JoinTypes joinType,
			//		   const Expr &expr)
			//{
			//	auto conj = std::make_tuple (_queryHelper, queryHelper2);
			//	return Queryable<decltype (conj)>
			//	{
			//		conj, _JoinStr (joinType) + _GetFromSql () +
			//			" on " + expr.expr, _pMapper };
			//}

			// Retrieve Select Result
			std::vector<QueryResult> ToVector ()
			{
				std::vector<QueryResult> ret;
				_pMapper->_Select (_queryHelper, ret, _GetFromSql ());
				return std::move (ret);
			}

			std::list<QueryResult> ToList ()
			{
				std::list<QueryResult> ret;
				_pMapper->_Select (_queryHelper, ret, _GetFromSql ());
				return std::move (ret);
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

		protected:
			ORMapper *_pMapper;
			const QueryResult &_queryHelper;

			std::string _querySource;
			std::string _sqlWhere;
			std::string _sqlOrderBy;
			std::string _sqlLimit, _sqlOffset;

			inline std::string _GetFromSql () const
			{
				return _querySource + _sqlWhere +
					_sqlOrderBy + _sqlLimit + _sqlOffset;
			}

			// Todo

			//inline static std::string _JoinStr (
			//	Helper::JoinTypes joinType)
			//{
			//	switch (joinType)
			//	{
			//	case JoinTypes::Inner:
			//		return "inner join ";
			//	case JoinTypes::Left:
			//		return "left join ";
			//	case JoinTypes::Right:
			//		return "right join ";
			//	case JoinTypes::Full:
			//		return "full join ";
			//	default:
			//		throw std::runtime_error ("No Such type...");
			//	}
			//}
		};

		template <typename C>
		Queryable<C> Query (const C &queryHelper)
		{
			return Queryable<C> (queryHelper,
								 std::string (C::__TableName),
								 this);
		}

	private:
		BOT_ORM_Impl::SQLConnector _connector;

		template <typename C, typename Out>
		void _Select (C copy, Out &out,
					  const std::string &sqlFrom)
		{
			static_assert (std::is_copy_constructible<C>::value,
						   "It must be Copy Constructible");

			_connector.Execute ("select * from " + sqlFrom + ";",
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
				out.emplace_back (std::move (copy));
			});
		}

		std::string _Select (const std::string &func,
							 const std::string &sqlFrom)
		{
			std::string ret;
			_connector.Execute ("select " + func + " from " + sqlFrom + ";",
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

	namespace Helper
	{
		template <typename C>
		inline ORMapper::FieldExtractor<C> Field (const C &queryHelper)
		{
			return ORMapper::FieldExtractor<C> { queryHelper };
		}

		template <typename C>
		inline auto All (const C &)
		{
			return ORMapper::AggregateFunc<C> { "*" };
		}
		inline auto Count ()
		{
			return ORMapper::AggregateFunc<size_t> { "count (*)" };
		}
		template <typename T>
		inline auto Count (const ORMapper::Field<T> &field)
		{
			return ORMapper::AggregateFunc<size_t> { "count (" +
				field.fieldName + ")" };
		}
		template <typename T>
		inline auto Sum (const ORMapper::Field<T> &field)
		{
			return ORMapper::AggregateFunc<T> { "sum (" +
				field.fieldName + ")" };
		}
		template <typename T>
		inline auto Avg (const ORMapper::Field<T> &field)
		{
			return ORMapper::AggregateFunc<T> { "avg (" +
				field.fieldName + ")" };
		}
		template <typename T>
		inline auto Max (const ORMapper::Field<T> &field)
		{
			return ORMapper::AggregateFunc<T> { "max (" +
				field.fieldName + ")" };
		}
		template <typename T>
		inline auto Min (const ORMapper::Field<T> &field)
		{
			return ORMapper::AggregateFunc<T> { "min (" +
				field.fieldName + ")" };
		}
	}
}

#endif // !BOT_ORM_H