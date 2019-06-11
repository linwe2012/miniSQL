#pragma once
#include "sqldata.h"
#include "catalog-manager.h"

class SQLDataPrettyPrinter : public ISQLDataVisitor {
public:
	void Visit(SQLBigInt* i) override {
		(*os_) << i->Value() << '\t';
	}

	void Visit(SQLNull* i) override {
		(*os_) << "(null)" << '\t';
	}

	void Visit(SQLDouble* i) override {
		(*os_) << i->Value() << '\t';
	}

	void Visit(SQLTimeStamp* i) {
		// (*os_) << i->Value() << '\t';
	}

	void Visit(SQLString* i) {
		(*os_) << i->Value() << '\t';
	}

	void Bind(std::ostream* os) {
		os_ = os;
	}
	std::ostream* os_;
};

inline void PrintTables(CatalogManager& cat, const std::string& db, std::ostream* os)
{
	auto map = cat.ShowTables(db);
	if (map == nullptr) {
		(*os) << "(null)" << std::endl;
		return;
	}
	for (auto itr : *(map)) {
		auto& tb = itr.second;
		(*os) << "(database)" << tb.db_name
			<< "  (table)" << tb.table_name << std::endl;
		for (auto attrib : tb.attributes) {
			char aster = attrib.is_primary_key ? '*' : ' ';
			(*os) << aster << attrib.column_name << ": \t\t"
				<< attrib.comment << attrib.nullable << std::endl;
		}
	}
}