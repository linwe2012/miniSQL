#include "catalog-manager.h"
#include <iostream>

int main() {
	std::string db_name = "cata_test_db_name";
	std::string table_name = "cata_test_table_name";
	std::vector<ColumnInfo> columns;
	ColumnInfo column;
	column.name.column_name = "column1";
	column.name.db_name = db_name;
	column.name.table_name = table_name;
	column.comment = "colum1comment";
	column.max_length = 10;
	column.is_primary_key = 1;
	column.nullable = 0;
	column.type = 1;
	columns.push_back(column);

	column.name.column_name = "column2";
	column.name.db_name = db_name;
	column.name.table_name = table_name;
	column.comment = "colum2comment";
	column.max_length = 8;
	column.is_primary_key = 1;
	column.nullable = 0;
	column.type = 3;
	columns.push_back(column);

	column.name.column_name = "column3";
	column.name.db_name = db_name;
	column.name.table_name = table_name;
	column.comment = "colum3comment";
	column.max_length = 8;
	column.is_primary_key = 1;
	column.nullable = 0;
	column.type = 3;
	columns.push_back(column);

	column.name.column_name = "column4";
	column.name.db_name = db_name;
	column.name.table_name = table_name;
	column.comment = "colum4comment";
	column.max_length = 8;
	column.is_primary_key = 1;
	column.nullable = 1;
	column.type = 3;

	CatalogManager cm;
	BufferManager bm;
	FileId cata_file;
	PageId cata_page;
	const char* file_name = "./cata_test";
	cata_file = bm.NewFile(file_name);
	cata_page = bm.AllocatePageAfter(cata_file, 0);
	cm.Initialize(&bm, cata_file, cata_page);
	cm.NewDatabase(db_name);
	cm.NewTable(db_name, table_name, columns);
	//cm.NewColumn(column);
	MetaData& meta = cm.FetchTable(db_name,table_name);
	std::cout << meta.db_name<< " " << meta.table_name << std::endl;
	for (int i = 0; i < meta.attributes.size(); i++) {
		std::cout << meta.attributes[i].column_name << "#" << meta.attributes[i].comment << "#" << meta.attributes[i].id << "#" << meta.attributes[i].type << std::endl;
	}
	for (int i = 0; i < meta.primary_keys.size(); i++)
		std::cout << meta.primary_keys[i];
	std::cout << std::endl;
}
