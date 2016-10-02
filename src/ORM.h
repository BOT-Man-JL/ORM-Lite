#ifndef BOT_ORM_H
#define BOT_ORM_H

#include <functional>
#include <vector>
#include <string>
#include <typeindex>
#include "SQL.h"

#define ORHOOK(...) friend class BOT_ORM::ORHook;\
void Hook (BOT_ORM::ORHook &hook)\
{\
	hook.Hook (__VA_ARGS__);\
}

namespace BOT_ORM
{
	class ORHook
	{
	public:
		ORHook () {}
		ORHook (std::string serialized)
			: _serialized (serialized) {}

		template <typename... Args>
		inline void Hook (Args & ... args)
		{
			return _Hook (args...);
		}

		std::vector<std::pair<std::string, std::type_index>> val_type;

	private:
		template <typename T>
		void _Hook (T &property)
		{
			static_assert (std::is_arithmetic<T>::value, "T must be arithmetic");
			if (_serialized.empty ())
				val_type.emplace_back (std::to_string (property), typeid (T));
			else
			{
				size_t pos = 0;
				if ((pos = _serialized.find ('\0', 0)) != std::string::npos)
				{
					if (typeid (T) == typeid (int))
						property = std::stoi (_serialized.substr (0, pos));
					else if (typeid (T) == typeid (double))
						property = std::stof (_serialized.substr (0, pos));
					_serialized.erase (0, pos + 1);
				}
			}
		}

		void _Hook (std::string &property)
		{
			if (_serialized.empty ())
				val_type.emplace_back ("'" + property + "'", typeid (std::string));
			else
			{
				size_t pos = 0;
				if ((pos = _serialized.find ('\0', 0)) != std::string::npos)
				{
					property = _serialized.substr (0, pos);
					_serialized.erase (0, pos + 1);
				}
			}
		}

		template <typename T, typename... Args>
		void _Hook (T &property, Args & ... args)
		{
			_Hook (property);
			_Hook (args...);
		}

		template <typename... Args>
		void _Hook (std::string &property, Args & ... args)
		{
			_Hook (property);
			_Hook (args...);
		}

		std::string _serialized;
	};

	template <typename C>
	class ORMapper
	{
	public:
		ORMapper (const std::string &dbName,
				  const std::string &tblName)
			: _dbName (dbName), _tblName (tblName)
		{}

		const std::string &ErrMsg () const
		{
			return _errMsg;
		}

		bool CreateTbl ()
		{
			ORHook orHook;
			C ().Hook (orHook);

			auto index = 0;
			std::vector<std::string> strFmts;
			for (const auto &i : orHook.val_type)
			{
				const auto &typeId = i.second;
				std::string typeFmt;
				if (typeId == typeid (int))
					typeFmt = "int";
				else if (typeId == typeid (double))
					typeFmt = "decimal";
				else if (typeId == typeid (std::string))
					typeFmt = "varchar";
				else
					return false;

				strFmts.push_back ("var" + std::to_string (index++) +
								   " " + typeFmt + ",");
			}

			strFmts.front ().pop_back ();
			strFmts.front () += " primary key,";
			strFmts.back ().pop_back ();

			std::string strFmt;
			for (auto &str : strFmts)
				strFmt += std::move (str);

			try
			{
				BOT_SQL::SQLConnector connector (_dbName);
				connector.CreateTable (_tblName, strFmt);
				return true;
			}
			catch (const std::exception &ex)
			{
				_errMsg = ex.what ();
				return false;
			}
		}

		bool DropTbl ()
		{
			try
			{
				BOT_SQL::SQLConnector connector (_dbName);
				connector.DropTable (_tblName);
				return true;
			}
			catch (const std::exception &ex)
			{
				_errMsg = ex.what ();
				return false;
			}
		}

		bool Insert (C &value)
		{
			return HandleCUD (value,
							  [&] (BOT_SQL::SQLConnector &connector,
								   const ORHook &orHook)
			{
				std::string strIns;
				for (const auto &i : orHook.val_type)
				{
					const auto &val = i.first;
					strIns += val + ",";
				}
				strIns.pop_back ();

				connector.InsertValue (_tblName, strIns);
			});
		}

		bool Delete (C &value)
		{
			return HandleCUD (value,
							  [&] (BOT_SQL::SQLConnector &connector,
								   const ORHook &orHook)
			{
				const auto &valKey = orHook.val_type.front ().first;
				std::string strDel = "var0=" + valKey;

				connector.DeleteValue (_tblName, strDel);
			});
		}

		bool Update (C &value)
		{
			return HandleCUD (value,
							  [&] (BOT_SQL::SQLConnector &connector,
								   const ORHook &orHook)
			{
				std::string strUpd;
				for (size_t i = 1; i < orHook.val_type.size (); i++)
				{
					const auto &val = orHook.val_type[i].first;
					strUpd += "var" + std::to_string (i) +
						"=" + val + ",";
				}
				strUpd.pop_back ();

				const auto &keyKey = orHook.val_type.front ().first;
				std::string strCond = "var0=" + keyKey;

				connector.UpdateValue (_tblName, strUpd, strCond);
			});
		}

		bool Query (std::vector<C> &out,
					const std::string &where)
		{
			try
			{
				BOT_SQL::SQLConnector connector (_dbName);
				connector.QueryValue (_tblName, "*", where,
									  [&] (int argc, char **argv, char **)
				{
					std::string serialized;
					for (size_t i = 0; i < argc; i++)
					{
						serialized += argv[i];
						serialized += char (0);
					}

					C obj;
					obj.Hook (ORHook (serialized));
					out.push_back (std::move (obj));
				});
				return true;
			}
			catch (const std::exception &ex)
			{
				_errMsg = ex.what ();
				return false;
			}
		}

	private:
		std::string _dbName, _tblName;
		std::string _errMsg;

		bool HandleCUD (C &value,
						std::function <void (
							BOT_SQL::SQLConnector &,
							const ORHook &)> fn)
		{
			ORHook orHook;
			value.Hook (orHook);
			try
			{
				BOT_SQL::SQLConnector connector (_dbName);
				fn (connector, orHook);
				return true;
			}
			catch (const std::exception &ex)
			{
				_errMsg = ex.what ();
				return false;
			}
		}
	};
}

#endif // !BOT_ORM_H