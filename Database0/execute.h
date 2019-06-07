#pragma once

#include "parser.h"
#include "buffer-manager.h"
#include "index-manager.h"
#include "catalog-manager.h"
#include "record-manager.h"

#include "logger.h"

struct ExecuteContext {
	std::string db;
	BufferManager* bm;
	IndexManager* index;
	CatalogManager* cat;
};


Result Execute(ExecuteContext ec, QueryClause q) {
	if (q.type == Token::kSelect) {

	}
}


std::vector<std::string> Explode(const std::string& s, char c) {
	std::string buff{ "" };
	std::vector<std::string> v;

	for (auto n : s)
	{
		if (n != c)
		{
			buff += n;
		}
		else if (n == c && buff != "") {
			v.push_back(std::move(buff));
			buff.clear();
		}
	}
	if (buff != "")
	{
		v.push_back(std::move(buff));
	}

	return v;
}


void DeduceColumnInfo(ExecuteContext& e, QueryClause& q) {
	for (auto& table : q.vectables) {
		if (table.db_name.size() == 0) {
			table.db_name = e.db;
		}

		q.tables[UnqiueTable{ table.db_name, table.name }] = &table;
		
		{
			auto itr = q.tables.find(UnqiueTable{
				table.db_name,
				table.alias
				}
			);

			if (itr != q.tables.end()) {
				console.warn("Table alias collide with previous table name or alias: ",
					itr->first.db, ".", itr->first.name);
			}

			q.tables[UnqiueTable{ table.db_name, table.alias }] = &table;
		};
	}

	for (auto var : q.variables) {
		std::vector<std::string> names = Explode(var->name(), '.');
		if (names.size() > 3) {
			console.warn("Too many specifiers in column name: ", var->name());
		}
		ColumnName col;
		col.column_name = names.back(); names.pop_back();
		
		if (names.size()) {
			col.table_name = names.back(); names.pop_back();
		}
		else {
			
		}

		if (names.size()) {
			col.db_name = names.back(); names.pop_back();
		}


	}

}





