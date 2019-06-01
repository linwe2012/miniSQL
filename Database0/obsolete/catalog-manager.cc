#include "catalog-manager.h"

const Attribute& CatalogManager::FetchColumn(ColumnName col_name)
{
	auto maybe = FetchColumnMaybe(col_name);
	if (maybe.IsNil()) {
		throw std::invalid_argument("column not exits");
	}
	return *maybe;
}

void CatalogManager::NewDatabase(const std::string& db_name) {
	if (tables_.find(db_name) == tables_.end()) {
		throw std::invalid_argument("Database already exits");
	}

	(void)tables_[db_name];
}

void CatalogManager::NewTable(const std::string& db_name,
	                          const std::string& table_name, 
	                          std::vector<ColumnInfo> columns) {
	if (columns.size() == 0) {
		throw std::invalid_argument("Expected at least 1 column in db");
	}

	MetaData& meta = AllocateEmptyTable(db_name, table_name);
	meta.db_name = db_name;
	meta.table_name = table_name;
	meta.file = file_;


	for (auto& col : columns) {
		auto attrib = AllocateAttribute();
		SetUpAttribute(col, attrib, meta);
	}
}

void CatalogManager::NewView(const std::string& db_name,
	                         const std::string& table_name, 
	                         std::vector<ColumnInfo> columns) {


	if (columns.size() == 0) {
		throw std::invalid_argument("Expected at least 1 column in db");
	}

	MetaData& meta = AllocateEmptyTable(db_name, table_name);

	for (auto& col : columns) {
		auto attrib = FetchColumnMaybe(col.name);
		if (attrib.IsNil()) {
			throw std::invalid_argument("column not exits");
		}
		SetUpAttribute(col, attrib, meta);
	}
}

void CatalogManager::NewColumn(ColumnInfo col_name)
{
	if (!FetchColumnMaybe(col_name.name).IsNil()) {
		throw std::invalid_argument("column already exits");
	}

	auto& meta = FetchTable(col_name.name.db_name, col_name.name.table_name);
	auto attrib = AllocateAttribute();
	SetUpAttribute(col_name, attrib, meta);
}

Maybe<Attribute> CatalogManager::AllocateAttribute() {
	auto attrib = new Attribute();
	attrib->id = latest_id_;
	attributes_[latest_id_].attribute = attrib;

	return Maybe(&attributes_[latest_id_++].attribute);
}

void CatalogManager::DeallocateAttribute(Maybe<Attribute> attrib) {
	if (attrib.IsNil()) {
		return;
	}

	delete  (*attrib.location());
	*attrib.location() = nullptr;
}

const Maybe<Attribute> CatalogManager::FetchColumnMaybe(const ColumnName& col)
{
	auto database_itr = tables_.find(col.db_name);

	if (database_itr == tables_.end()) {
		throw std::invalid_argument("Database not exist");
	}

	auto& database = database_itr->second;
	auto table_itr = database.find(col.table_name);

	if (table_itr == database.end()) {
		throw std::invalid_argument("Table does not exits");
	}

	auto& table = table_itr->second;

	auto attrib_itr = table.attributes_map.find(col.column_name);
	if (attrib_itr == table.attributes_map.end()) {
		return Maybe<Attribute>();
	}

	auto& attrib = attrib_itr->second;

	if (attrib.IsNil()) {
		throw std::invalid_argument("Referencing deleted column");
	}


	return attrib;
}

MetaData& CatalogManager::AllocateEmptyTable(const std::string& db_name, const std::string& table_name)
{
	auto database_itr = tables_.find(db_name);

	if (database_itr == tables_.end()) {
		throw std::invalid_argument("Database not exists");
	}

	auto& database = database_itr->second;
	auto table_itr = database.find(table_name);

	if (table_itr != database.end()) {
		throw std::invalid_argument("Table already exits");
	}

	return database[table_name];
}

void CatalogManager::SetUpAttribute(const ColumnInfo& col, Maybe<Attribute> attrib, MetaData& meta)
{
	attrib->column_name = col.name.column_name;
	attrib->comment = col.comment;
	attrib->type = col.type;

	attrib->file = file_;

	attrib->first_page = bm.AllocatePageAfter(file_, 0);
	attrib->max_length = col.max_length;
	attrib->nullable = col.nullable;
	attrib->is_primary_key = col.is_primary_key;

	meta.attributes.push_back(attrib);
	meta.attributes_map[col.name.column_name] = attrib;

	if (col.is_primary_key) {
		meta.primary_keys.push_back(meta.attributes.size() - 1);
	}
}


