
// ORM Lite
// An ORM for SQLite in C++ 14
// https://github.com/BOT-Man-JL/ORM-Lite
// BOT Man, 2017

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

// std::shared_ptr
#include <memory>

// for Field Name Extractor
#include <cctype>

// for SQL Connector
#include <thread>
#include <functional>

// SQLite 3 Dependency
#include "sqlite3.h"

// Nullable Module
#include "nullable.h"

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

#define BAD_COLUMN_COUNT "Bad Column Count"
#define NULL_DESERIALIZE "Get Null Value"

#define NO_FIELD "No Such Field for current Extractor"
#define NOT_SAME_TABLE "Fields are NOT from the Same Table"

// Helpers

namespace BOT_ORM_Impl
{
    // Naive SQL Driver (Todo: Improved Later)

    class SQLConnector
    {
    public:
        SQLConnector (const std::string &fileName)
        {
            if (sqlite3_open (fileName.c_str (), &db) != SQLITE_OK)
                throw std::runtime_error (
                    std::string ("SQL error: Can't open database '")
                    + sqlite3_errmsg (db) + "'");
        }

        ~SQLConnector ()
        {
            sqlite3_close (db);
        }

        void Execute (const std::string &cmd)
        {
            char *zErrMsg = 0;
            int rc = SQLITE_OK;

            for (size_t iTry = 0; iTry < MAX_TRIAL; iTry++)
            {
                rc = sqlite3_exec (db, cmd.c_str (), 0, 0, &zErrMsg);
                if (rc != SQLITE_BUSY)
                    break;

                std::this_thread::sleep_for (
                    std::chrono::microseconds (20));
            }

            if (rc != SQLITE_OK)
            {
                auto errStr = std::string ("SQL error: '") + zErrMsg
                    + "' at '" + cmd + "'";
                sqlite3_free (zErrMsg);
                throw std::runtime_error (errStr);
            }
        }

        void ExecuteCallback (const std::string &cmd,
            std::function<void (int, char **)>
            callback)
        {
            char *zErrMsg = 0;
            int rc = SQLITE_OK;

            auto callbackParam = std::make_pair (&callback, std::string {});

            for (size_t iTry = 0; iTry < MAX_TRIAL; iTry++)
            {
                rc = sqlite3_exec (db, cmd.c_str (), CallbackWrapper,
                    &callbackParam, &zErrMsg);
                if (rc != SQLITE_BUSY)
                    break;

                std::this_thread::sleep_for (
                    std::chrono::microseconds (20));
            }

            if (rc == SQLITE_ABORT)
            {
                auto errStr = "SQL error: '" + callbackParam.second +
                    "' at '" + cmd + "'";
                sqlite3_free (zErrMsg);
                throw std::runtime_error (errStr);
            }
            else if (rc != SQLITE_OK)
            {
                auto errStr = std::string ("SQL error: '") + zErrMsg
                    + "' at '" + cmd + "'";
                sqlite3_free (zErrMsg);
                throw std::runtime_error (errStr);
            }
        }

    private:
        sqlite3 *db;
        constexpr static size_t MAX_TRIAL = 16;

        static int CallbackWrapper (
            void *callbackParam, int argc, char **argv, char **)
        {
            auto pParam = static_cast<std::pair<
                std::function<void (int, char **)> *,
                std::string
            >*>(callbackParam);

            try
            {
                pParam->first->operator()(argc, argv);
                return 0;
            }
            catch (const std::exception &ex)
            {
                pParam->second = ex.what ();
                return 1;
            }
        }
    };

    // Helper - Field Type Checker

    template <typename T>
    struct TypeString
    {
        constexpr static const char *typeStr =
            (std::is_integral<T>::value &&
                !std::is_same<T, char>::value &&
                !std::is_same<T, wchar_t>::value &&
                !std::is_same<T, char16_t>::value &&
                !std::is_same<T, char32_t>::value &&
                !std::is_same<T, unsigned char>::value)
            ? " integer not null"
            : (std::is_floating_point<T>::value)
            ? " real not null"
            : (std::is_same<T, std::string>::value)
            ? " text not null"
            : nullptr;

        static_assert (typeStr != nullptr, BAD_TYPE);
    };

    template <typename T>
    struct TypeString <BOT_ORM::Nullable<T>>
    {
        constexpr static const char *typeStr =
            (std::is_integral<T>::value &&
                !std::is_same<T, char>::value &&
                !std::is_same<T, wchar_t>::value &&
                !std::is_same<T, char16_t>::value &&
                !std::is_same<T, char32_t>::value &&
                !std::is_same<T, unsigned char>::value)
            ? " integer"
            : (std::is_floating_point<T>::value)
            ? " real"
            : (std::is_same<T, std::string>::value)
            ? " text"
            : nullptr;

        static_assert (typeStr != nullptr, BAD_TYPE);
    };

    // Serialization Helper

    struct SerializationHelper
    {
        template <typename T>
        static inline
            std::enable_if_t<TypeString<T>::typeStr == nullptr, bool>
            Serialize (std::ostream &, const T &)
        {}
        template <typename T>
        static inline
            std::enable_if_t<TypeString<T>::typeStr != nullptr, bool>
            Serialize (std::ostream &os, const T &value)
        {
            os << value;
            return true;
        }

        static inline bool Serialize (std::ostream &os,
            const std::string &value)
        {
            os << "'" << value << "'";
            return true;
        }

        template <typename T>
        static inline bool Serialize (
            std::ostream &os,
            const BOT_ORM::Nullable<T> &value)
        {
            if (value == nullptr)
                return false;
            return Serialize (os, value.Value ());
        }
    };

    // Deserialization Helper

    struct DeserializationHelper
    {
        template <typename T>
        static inline std::enable_if_t<TypeString<T>::typeStr == nullptr>
            Deserialize (T &, const char *)
        {}
        template <typename T>
        static inline std::enable_if_t<TypeString<T>::typeStr != nullptr>
            Deserialize (T &property, const char *value)
        {
            if (value) std::istringstream { value } >> property;
            else throw std::runtime_error (NULL_DESERIALIZE);
        }

        static inline void Deserialize (std::string &property,
            const char *value)
        {
            if (value) property = value;
            else throw std::runtime_error (NULL_DESERIALIZE);
        }

        template <typename T>
        static inline void Deserialize (
            BOT_ORM::Nullable<T> &property, const char *value)
        {
            if (value)
            {
                T val;
                Deserialize (val, value);
                property = val;
            }
            else
                property = nullptr;
        }
    };

    // Injection Helper

    class InjectionHelper
    {
        static std::vector<std::string> ExtractFieldName (std::string input)
        {
            std::vector<std::string> ret;
            std::string tmpStr;

            for (char ch : std::move (input) + ",")
            {
                if (isalnum (ch) || ch == '_')
                    tmpStr += ch;
                else if (ch == ',')
                {
                    if (!tmpStr.empty ())
                        ret.push_back (tmpStr);
                    tmpStr.clear ();
                }
            }
            return ret;
        };

    public:
        // Checking Injection
        template <typename T> class HasInjected
        {
            template <typename...> struct make_void { using type = void; };
            template <typename... _Types>
            using void_t = typename make_void<_Types...>::type;

            template <typename, typename = void_t<>>
            struct Test : std::false_type {};
            template <typename U>
            struct Test <U, void_t<decltype (U::__TableName)>>
                : std::true_type
            {};

        public:
            static constexpr bool value = Test<T>::value;

            static_assert (value, NO_ORMAP);
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
            static const auto fieldNames = ExtractFieldName (C::__FieldNames);
            return fieldNames;
        }

        // Table Name Proxy
        template <typename C>
        static inline const std::string &TableName (const C &)
        {
            static const std::string tableName { C::__TableName };
            return tableName;
        }
    };

    // Unpacking Tricks :-)
    // http://stackoverflow.com/questions/26902633/how-to-iterate-over-a-tuple-in-c-11/26902803#26902803
    // - To avoid the unspecified order,
    //   brace-enclosed initializer lists can be used,
    //   which guarantee strict left-to-right order of evaluation.
    // - To avoid the need for a not void return type,
    //   the comma operator can be used to
    //   always yield 1 in each expansion element.

    using Expander = int[];
}

namespace BOT_ORM
{
    namespace Expression
    {
        // SetExpr

        struct SetExpr
        {
            SetExpr (std::string field_op_val)
                : _expr { std::move (field_op_val) }
            {}

            const std::string &ToString () const
            {
                return _expr;
            }

            inline SetExpr operator && (const SetExpr &rhs) const
            {
                return SetExpr { _expr + "," + rhs._expr };
            }

        private:
            std::string _expr;
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
                BOT_ORM_Impl::SerializationHelper::
                    Serialize (os << this->fieldName << "=", value);
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
                BOT_ORM_Impl::SerializationHelper::
                    Serialize (os << this->fieldName << "=", value);
                return SetExpr { os.str () };
            }

            inline SetExpr operator = (std::nullptr_t)
            {
                return SetExpr { this->fieldName + "=null" };
            }
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
            Expr (const Selectable<T> &field, std::string op_val)
                : _exprs { { field.fieldName + op_val, field.tableName } }
            {}

            template <typename T>
            Expr (const Selectable<T> &field, std::string op, T value)
            {
                std::ostringstream os;
                BOT_ORM_Impl::SerializationHelper::
                    Serialize (os << field.fieldName << op, value);
                _exprs.emplace_back (os.str (), field.tableName);
            }

            template <typename T>
            Expr (const Field<T> &field1,
                std::string op,
                const Field<T> &field2)
                : _exprs
            {
                { field1.fieldName, field1.tableName },
                { std::move (op), nullptr },
                { field2.fieldName, field2.tableName }
            }
            {}

            std::string ToString () const
            {
                std::ostringstream out;
                for (const auto &expr : _exprs)
                {
                    if (expr.second != nullptr)
                        out << *(expr.second) << ".";
                    out << expr.first;
                }
                return out.str ();
            }

            inline Expr operator && (const Expr &rhs) const
            {
                return And_Or (rhs, " and ");
            }
            inline Expr operator || (const Expr &rhs) const
            {
                return And_Or (rhs, " or ");
            }

        private:
            std::list<std::pair<std::string, const std::string *>> _exprs;

            Expr And_Or (const Expr &rhs, std::string logOp) const
            {
                auto ret = *this;
                auto rigthExprs = rhs._exprs;
                ret._exprs.emplace_front ("(", nullptr);
                ret._exprs.emplace_back (std::move (logOp), nullptr);
                ret._exprs.splice (ret._exprs.cend (),
                    std::move (rigthExprs));
                ret._exprs.emplace_back (")", nullptr);
                return ret;
            }
        };

        // Field / Aggregate ? Value

        template <typename T>
        inline Expr operator == (const Selectable<T> &op, T value)
        {
            return Expr (op, "=", std::move (value));
        }

        template <typename T>
        inline Expr operator != (const Selectable<T> &op, T value)
        {
            return Expr (op, "!=", std::move (value));
        }

        template <typename T>
        inline Expr operator > (const Selectable<T> &op, T value)
        {
            return Expr (op, ">", std::move (value));
        }

        template <typename T>
        inline Expr operator >= (const Selectable<T> &op, T value)
        {
            return Expr (op, ">=", std::move (value));
        }

        template <typename T>
        inline Expr operator < (const Selectable<T> &op, T value)
        {
            return Expr (op, "<", std::move (value));
        }

        template <typename T>
        inline Expr operator <= (const Selectable<T> &op, T value)
        {
            return Expr (op, "<=", std::move (value));
        }

        // Field ? Field

        template <typename T>
        inline Expr operator == (const Field<T> &op1, const Field<T> &op2)
        {
            return Expr { op1 , "=", op2 };
        }

        template <typename T>
        inline Expr operator != (const Field<T> &op1, const Field<T> &op2)
        {
            return Expr { op1, "!=", op2 };
        }

        template <typename T>
        inline Expr operator > (const Field<T> &op1, const Field<T> &op2)
        {
            return Expr { op1 , ">", op2 };
        }

        template <typename T>
        inline Expr operator >= (const Field<T> &op1, const Field<T> &op2)
        {
            return Expr { op1, ">=", op2 };
        }

        template <typename T>
        inline Expr operator < (const Field<T> &op1, const Field<T> &op2)
        {
            return Expr { op1 , "<", op2 };
        }

        template <typename T>
        inline Expr operator <= (const Field<T> &op1, const Field<T> &op2)
        {
            return Expr { op1, "<=", op2 };
        }

        // Nullable Field == / != nullptr

        template <typename T>
        inline Expr operator == (const NullableField<T> &op, std::nullptr_t)
        {
            return Expr { op, " is null" };
        }

        template <typename T>
        inline Expr operator != (const NullableField<T> &op, std::nullptr_t)
        {
            return Expr { op, " is not null" };
        }

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
        {
            return Aggregate<size_t> { "count (*)" };
        }

        template <typename T>
        inline auto Count (const Field<T> &field)
        {
            return Aggregate<T> { "count", field };
        }

        template <typename T>
        inline auto Sum (const Field<T> &field)
        {
            return Aggregate<T> { "sum", field };
        }

        template <typename T>
        inline auto Avg (const Field<T> &field)
        {
            return Aggregate<T> { "avg", field };
        }

        template <typename T>
        inline auto Max (const Field<T> &field)
        {
            return Aggregate<T> { "max", field };
        }

        template <typename T>
        inline auto Min (const Field<T> &field)
        {
            return Aggregate<T> { "min", field };
        }
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
                (void) BOT_ORM_Impl::Expander
                {
                    0, (Extract (args), 0)...
                };
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
            BOT_ORM_Impl::SerializationHelper::
                Serialize (os << " default ", value);
            return Constraint { os.str (), field.fieldName };
        }

        static inline Constraint Check (
            const Expression::Expr &expr)
        {
            return Constraint { "check (" + expr.ToString () + ")" };
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

        // Apply 'Fn' to each of element of Tuple
        template <typename Fn, typename TupleType, std::size_t... I>
        static inline void TupleVisit_Impl (
            TupleType &tuple, Fn fn, std::index_sequence<I...>)
        {
            (void) BOT_ORM_Impl::Expander
            {
                0, ((void) fn (std::get<I> (tuple)), 0)...
            };
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
        inline Queryable Distinct () const &
        {
            auto ret = *this;
            ret._sqlSelect = "select distinct ";
            return ret;
        }
        inline Queryable Distinct () &&
        {
            (*this)._sqlSelect = "select distinct ";
            return std::move (*this);
        }

        // Where
        inline Queryable Where (const Expression::Expr &expr) const &
        {
            auto ret = *this;
            ret._sqlWhere = " where (" + expr.ToString () + ")";
            return ret;
        }
        inline Queryable Where (const Expression::Expr &expr) &&
        {
            (*this)._sqlWhere = " where (" + expr.ToString () + ")";
            return std::move (*this);
        }

        // Group By
        template <typename T>
        inline Queryable GroupBy (const Expression::Field<T> &field) const &
        {
            auto ret = *this;
            ret._sqlGroupBy = " group by " +
                BOT_ORM_Impl::QueryableHelper::FieldToSql (field);
            return ret;
        }
        template <typename T>
        inline Queryable GroupBy (const Expression::Field<T> &field) &&
        {
            (*this)._sqlGroupBy = " group by " +
                BOT_ORM_Impl::QueryableHelper::FieldToSql (field);
            return std::move (*this);
        }

        // Having
        inline Queryable Having (const Expression::Expr &expr) const &
        {
            auto ret = *this;
            ret._sqlHaving = " having " + expr.ToString ();
            return ret;
        }
        inline Queryable Having (const Expression::Expr &expr) &&
        {
            (*this)._sqlHaving = " having " + expr.ToString ();
            return std::move (*this);
        }

        // Limit
        inline Queryable Take (size_t count) const &
        {
            auto ret = *this;
            ret._sqlLimit = " limit " + std::to_string (count);
            return ret;
        }
        inline Queryable Take (size_t count) &&
        {
            (*this)._sqlLimit = " limit " + std::to_string (count);
            return std::move (*this);
        }

        // Offset
        inline Queryable Skip (size_t count) const &
        {
            auto ret = *this;
            if (ret._sqlLimit.empty ())
                ret._sqlLimit = " limit ~0";  // ~0 is a trick :-)
            ret._sqlOffset = " offset " + std::to_string (count);
            return ret;
        }
        inline Queryable Skip (size_t count) &&
        {
            if ((*this)._sqlLimit.empty ())
                (*this)._sqlLimit = " limit ~0";  // ~0 is a trick :-)
            (*this)._sqlOffset = " offset " + std::to_string (count);
            return std::move (*this);
        }

        // Order By
        template <typename T>
        inline Queryable OrderBy (
            const Expression::Field<T> &field) const &
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
        inline Queryable OrderBy (
            const Expression::Field<T> &field) &&
        {
            if ((*this)._sqlOrderBy.empty ())
                (*this)._sqlOrderBy = " order by " +
                BOT_ORM_Impl::QueryableHelper::FieldToSql (field);
            else
                (*this)._sqlOrderBy += "," +
                BOT_ORM_Impl::QueryableHelper::FieldToSql (field);
            return std::move (*this);
        }

        // Order By Desc
        template <typename T>
        inline Queryable OrderByDescending (
            const Expression::Field<T> &field) const &
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
        template <typename T>
        inline Queryable OrderByDescending (
            const Expression::Field<T> &field) &&
        {
            if ((*this)._sqlOrderBy.empty ())
                (*this)._sqlOrderBy = " order by " +
                BOT_ORM_Impl::QueryableHelper::FieldToSql (field) + " desc";
            else
                (*this)._sqlOrderBy += "," +
                BOT_ORM_Impl::QueryableHelper::FieldToSql (field) + " desc";
            return std::move (*this);
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
        inline auto Join (const C &,
            const Expression::Expr &,
            std::enable_if_t<
            !HasInjected<C>::value>
            * = nullptr) const
        {}
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
        inline auto LeftJoin (const C &,
            const Expression::Expr &,
            std::enable_if_t<
            !HasInjected<C>::value>
            * = nullptr) const
        {}
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
        {
            return _NewCompoundQuery (queryable, " union ");
        }
        inline Queryable UnionAll (Queryable queryable) const
        {
            return _NewCompoundQuery (queryable, " union all ");
        }
        inline Queryable Intersect (Queryable queryable) const
        {
            return _NewCompoundQuery (queryable, " intersect ");
        }
        inline Queryable Except (Queryable queryable) const
        {
            return _NewCompoundQuery (queryable, " excpet ");
        }

        // Get Result
        template <typename T>
        Nullable<T> Aggregate (const Expression::Aggregate<T> &agg) const
        {
            Nullable<T> ret;
            _connector->ExecuteCallback (_sqlSelect + agg.fieldName +
                _GetFromSql () + _GetLimit () + ";",
                [&] (int argc, char **argv)
            {
                if (argc != 1)
                    throw std::runtime_error (BAD_COLUMN_COUNT);

                BOT_ORM_Impl::DeserializationHelper::
                    Deserialize (ret, argv[0]);
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
        {
            return _sqlFrom + _sqlWhere + _sqlGroupBy + _sqlHaving;
        }

        // Return ORDER BY & LIMIT part for Query
        inline std::string _GetLimit () const
        {
            return _sqlOrderBy + _sqlLimit + _sqlOffset;
        }

        // Return a new Queryable Object
        template <typename... Args>
        inline Queryable<std::tuple<Args...>> _NewQuery (
            std::string sqlTarget,
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
                " on " + onExpr.ToString (),
                BOT_ORM_Impl::QueryableHelper::JoinToTuple (
                    _queryHelper, queryHelper2));
        }

        // Return a new Compound Queryable Object
        Queryable _NewCompoundQuery (const Queryable &queryable,
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
        inline void _Select (const C &, Out &out) const
        {
            auto copy = _queryHelper;
            _connector->ExecuteCallback (_sqlSelect + _sqlTarget +
                _GetFromSql () + _GetLimit () + ";",
                [&] (int argc, char **argv)
            {
                BOT_ORM_Impl::InjectionHelper::Visit (
                    copy, [argc] (auto & ... args)
                {
                    if (sizeof... (args) != argc)
                        throw std::runtime_error (BAD_COLUMN_COUNT);
                });

                BOT_ORM_Impl::InjectionHelper::Visit (
                    copy, [argv] (auto & ... args)
                {
                    size_t index = 0;
                    (void) BOT_ORM_Impl::Expander
                    {
                        0, (BOT_ORM_Impl::DeserializationHelper::
                            Deserialize (args, argv[index++]), 0)...
                    };
                });
                out.push_back (copy);
            });
        }

        // Select for Tuples
        template <typename Out, typename... Args>
        inline void _Select (const std::tuple<Args...> &, Out &out) const
        {
            auto copy = _queryHelper;
            _connector->ExecuteCallback (_sqlSelect + _sqlTarget +
                _GetFromSql () + _GetLimit () + ";",
                [&] (int argc, char **argv)
            {
                if (sizeof... (Args) != argc)
                    throw std::runtime_error (BAD_COLUMN_COUNT);

                size_t index = 0;
                BOT_ORM_Impl::QueryableHelper::TupleVisit (
                    copy, [argv, &index] (auto &val)
                {
                    BOT_ORM_Impl::DeserializationHelper::
                        Deserialize (val, argv[index++]);
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
            CreateTbl (const C &, const Args & ...)
        {}
        template <typename C, typename... Args>
        std::enable_if_t<HasInjected<C>::value>
            CreateTbl (const C &entity,
                const Args & ... constraints)
        {
            const auto &fieldNames =
                BOT_ORM_Impl::InjectionHelper::FieldNames (entity);
            std::unordered_map<std::string, std::string> fieldFixes;

            auto addTypeStr = [&fieldNames, &fieldFixes] (
                const auto &arg, size_t index)
            {
                // Why addTypeStr?
                // Walkaround the 'undefined reference' in gcc/clang
                constexpr const char *typeStr = BOT_ORM_Impl::TypeString<
                    std::remove_cv_t<std::remove_reference_t<decltype(arg)>>
                >::typeStr;
                fieldFixes.emplace (fieldNames[index], typeStr);
            };
            (void) addTypeStr;

            BOT_ORM_Impl::InjectionHelper::Visit (
                entity, [&addTypeStr] (const auto & ... args)
            {
                size_t index = 0;
                (void) BOT_ORM_Impl::Expander
                {
                    0, (addTypeStr (args, index++), 0)...
                };
            });
            fieldFixes[fieldNames[0]] += " primary key";

            std::string tableFixes;
            _GetConstraints (tableFixes, fieldFixes, constraints...);

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
            DropTbl (const C &)
        {}
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
            Insert (const C &, bool = true)
        {}
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
            InsertRange (const In &, bool = true)
        {}
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
            Update (const C &)
        {}
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
            UpdateRange (const In &)
        {}
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
            Update (const C &,
                const Expression::SetExpr &,
                const Expression::Expr &)
        {}
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
                whereExpr.ToString () + ";");
        }

        template <typename C>
        std::enable_if_t<!HasInjected<C>::value>
            Delete (const C &)
        {}
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
                entity, [&os] (const auto &primaryKey,
                    const auto & ... dummy)
            {
                // Why 'eatdummy'?
                // Walkaround 'fatal error c1001:
                // an internal error has occurred in the compiler.' on MSVC 14
                auto eatdummy = [] (const auto &) {};
                (void) eatdummy;

                // Why 'dummy'?
                // Walkaround 'template argument deduction/substitution failed'
                // on gcc 5.4
                (void) BOT_ORM_Impl::Expander
                {
                    0, (eatdummy (dummy), 0)...
                };

                if (!BOT_ORM_Impl::SerializationHelper::
                    Serialize (os, primaryKey))
                    os << "null";
            });
            os << ";";

            _connector->Execute (os.str ());
        }

        template <typename C>
        std::enable_if_t<!HasInjected<C>::value>
            Delete (const C &,
                const Expression::Expr &)
        {}
        template <typename C>
        std::enable_if_t<HasInjected<C>::value>
            Delete (const C &entity,
                const Expression::Expr &whereExpr)
        {
            _connector->Execute (
                "delete from " +
                BOT_ORM_Impl::InjectionHelper::TableName (entity) +
                " where " +
                whereExpr.ToString () + ";");
        }

        template <typename C>
        std::enable_if_t<!HasInjected<C>::value, Queryable<C>>
            Query (C)
        {}
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
                    if (BOT_ORM_Impl::SerializationHelper::
                        Serialize (osVal, val))
                    {
                        os << fieldNames[index] << ",";
                        osVal << ",";
                        anyField = true;
                    }
                };

                // Priamry Key
                if (withId &&
                    BOT_ORM_Impl::SerializationHelper::
                    Serialize (osVal, primaryKey))
                {
                    os << fieldNames[0] << ",";
                    osVal << ",";
                    anyField = true;
                }

                // The Rest
                size_t index = 1;

                (void) BOT_ORM_Impl::Expander
                {
                    0, (serializeField (args, index++), 0)...
                };
                (void) serializeField;

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
                    if (!BOT_ORM_Impl::SerializationHelper::
                        Serialize (os, val))
                        os << "null";
                    os << ",";
                };

                // The Rest
                size_t index = 1;

                (void) BOT_ORM_Impl::Expander
                {
                    0, (serializeField (args, index++), 0)...
                };
                (void) serializeField;

                os.seekp (os.tellp () - std::streamoff (1));

                // Primary Key
                os << " where " << fieldNames[0] << "=";
                if (!BOT_ORM_Impl::SerializationHelper::
                    Serialize (os, primaryKey))
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
            Extract (const C &)
        {}
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
                (void) BOT_ORM_Impl::Expander
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
            (void) BOT_ORM_Impl::Expander
            {
                0, (Extract (args), 0)...
            };
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
#undef BAD_COLUMN_COUNT
#undef NULL_DESERIALIZE
#undef NOT_SAME_TABLE

#endif // !BOT_ORM_H
