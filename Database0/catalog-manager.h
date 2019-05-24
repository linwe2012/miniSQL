#pragma once
#include "attribute.h"

struct ColumnName {
	const std::string& db_name;
	const std::string& table_name;
	const std::string& column_name;
};


class CatalogManager {
	// 各种错误比如重复命名的table反正扔一个std::exception 就好了
	// https://en.cppreference.com/w/cpp/error/exception <-- exception 的列表
	const MetaData& FetchTable(const std::string& db_name,
		const std::string& table_name);

	const Attribute& FetchColumn(ColumnName col_name);


	void NewTable(const MetaData& meta);

	void DropTable(const std::string& db_name,
		const std::string& table_name);

	void NewColumn(ColumnName col_name);

	void DropColumn(ColumnName col_name);
};