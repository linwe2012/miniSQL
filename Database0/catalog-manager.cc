#include "catalog-manager.h"
#include <stdexcept>
#include <cmath>
#include <utility>
#include <cstring>
#include<iostream>


void CatalogManager::Initialize(BufferManager* buffer_manager, FileId file, PageId page) {
	bm = buffer_manager;
	file_ = file;
	page_ = page;

	auto itr = bm->GetPage<char *>(file, page);

	while (!itr.IsEnd())
	{
		MetaData meta;
		DeserializeMeta(itr, meta);
		tables_[meta.db_name][meta.table_name] = std::move(meta);
	}
}

std::map<std::string, MetaData>* CatalogManager::ShowTables(const std::string& db_name) {
	std::map<std::string, std::map<std::string, MetaData>>::iterator iter = tables_.find(db_name);
	if (iter == tables_.end())
		return nullptr;
	return &iter->second;
}

CatalogManager::~CatalogManager()
{
	auto itr = bm->GetPage<char*>(file_, page_);
	itr.ClearInPage();
	itr.MoveToPageEnd();
	while (!itr.IsEnd())
	{
		++itr;
		itr.MoveToPageEnd();
		itr.ClearInPage();
	}
	
	itr = bm->GetPage<char*>(file_, page_);
	for (auto& db : tables_) {
		for (auto& tb : db.second) {
			SerializeOneTable(itr, tb.second);
		}
	}
}


void CatalogManager::NewDatabase(const std::string& db_name) {
	std::map<std::string, std::map<std::string, MetaData>>::iterator f = tables_.find(db_name);
	if(f!=tables_.end())
		throw std::invalid_argument("exsisted database!");

	tables_[db_name]; // std::map automacitally create this slot
}


void CatalogManager::InsertComlumFromInfo(MetaData& meta, ColumnInfo& col) {
	int id = 0;
	Attribute attrib;
	attrib.id = id++;
	attrib.type = col.type;
	attrib.column_name = col.name.column_name;
	attrib.comment = col.comment;
	attrib.file = meta.file;
	attrib.first_page = bm->AllocatePageAfter(meta.file, 0);
	attrib.max_length = col.max_length;
	attrib.is_primary_key = col.is_primary_key;
	attrib.nullable = col.nullable;
	meta.attributes.push_back(attrib);
	meta.attributes_map[attrib.column_name] = &meta.attributes[meta.attributes.size() - 1];
	if (attrib.is_primary_key) {
		meta.primary_keys.push_back(meta.attributes.size() - 1);
	}

}


void CatalogManager::NewTable(const std::string& db_name,const std::string& table_name, std::vector<ColumnInfo> columns) {

	//if exsists a homonymic table
	if (FindTable(db_name,table_name)) 
		throw std :: invalid_argument("exsisted table!");

	//metadata census
	tables_[db_name][table_name] = MetaData();

	MetaData& meta = tables_[db_name][table_name];
	meta.db_name = db_name;
	meta.table_name = table_name;
	meta.file = file_;
	meta.attributes.reserve(columns.size());
	for (auto& iter : columns) {
		InsertComlumFromInfo(meta, iter);
	}
}

void CatalogManager::DropTable(const std::string& db_name, const std::string& table_name) {
	if(!FindTable(db_name,table_name))
		throw std::invalid_argument("no such table!");

	auto v_meta = bm->GetPage<char*>(file_, page_);
	for (int i = 0; i < tables_.size(); i++) {
		MetaData meta_tmp;
		DeserializeMeta(v_meta, meta_tmp);
		if (db_name == meta_tmp.db_name && table_name == meta_tmp.table_name) {
		//在page里delete这个
			break;
		}
		++v_meta;
	}
	std::map<std::string, std::map<std::string, MetaData>>::iterator iter;
	iter = tables_.find(db_name);
	std::map<std::string, MetaData>::iterator iiter;
	iiter = iter->second.find(table_name);
	(iter->second).erase(iiter);
}

MetaData& CatalogManager::FetchTable(const std::string& db_name, const std::string& table_name) {
	auto iter = tables_.find(db_name);
	if (iter == tables_.end()) {
		throw std::invalid_argument("database not found");
	}
	
	auto& db = iter->second;
	auto tb_itr = db.find(table_name);
	if (tb_itr == db.end()) {
		throw std::invalid_argument("table not found");
	}

	return tb_itr->second;
}

Attribute& CatalogManager::FetchColumn(ColumnName col_name) {
	MetaData& table = FetchTable(col_name.db_name, col_name.table_name);
	auto itr = table.attributes_map.find(col_name.column_name);
	if (itr == table.attributes_map.end()) {
		throw std::invalid_argument("no such column!");
	}
	return *(itr->second);
}

int CatalogManager::FindTable(const std::string& db_name, const std::string& table_name) {
	auto iter = tables_.find(db_name);
	if (iter == tables_.end())
		return 0;

	auto& tbs = iter->second;
	if (tbs.find(table_name) == tbs.end())
		return 0;
	return 1;
}

void CatalogManager::NewColumn(ColumnInfo col_name) {
	auto& meta = FetchTable(col_name.name.db_name, col_name.name.table_name);
	if (meta.attributes_map.find(col_name.name.column_name) == meta.attributes_map.end()) {
		throw std::invalid_argument("existed column!");
	}

	InsertComlumFromInfo(meta, col_name);
}

void CatalogManager::DropColumn(ColumnName name) {
	auto& meta = FetchTable(name.db_name, name.table_name);
	auto itr = meta.attributes_map.find(name.column_name);
	if (itr == meta.attributes_map.end()) {
		throw std::invalid_argument("dropping column not exit!!!");
	}

	//TODO(L): Here we use a trick to do this, is there any other way?
	size_t offset = itr->second - meta.attributes.begin().operator->();
	meta.attributes.erase(meta.attributes.begin() + offset);
	meta.attributes_map.erase(itr);
}


/*std::string CatalogManager::meta2str(MetaData meta) {
	std::string str = "";
	str = meta.db_name + "#" + meta.table_name + "#" + itos(meta.file) + "#" + itos(meta.attributes.size());
	for (int i = 0; i < meta.attributes.size(); i++) {
		str = str + "#" + attr2str(meta.attributes[i]);
	}
}

MetaData CatalogManager::str2meta(std::string str) {
	MetaData meta;
	std::string::size_type pos1, pos2;
	pos2 = str.find("#");
	pos1 = 0;
	meta.db_name = str.substr(pos1, pos2 - pos1);
	pos1 = pos2 + 1;
	pos2 = s.find(" ", pos1);
	meta.table_name = str.substr(pos1, pos2 - pos1);
	pos1 = pos2 + 1;
	pos2 = s.find("#", pos1);
	int size = stoi(str.substr(pos1, pos2 - pos1));
	for (int i = 0; i < size; i++) {
		pos1 = pos2 + 1;
		pos2 = s.find("#", pos1);
		std::string str_tmp = str.substr(pos1, pos2 - pos1);
		Attribute attr_tmp = str2attr(str_tmp);
		meta.attributes.push_back(attr_tmp);
		meta.attributes_map.insert(std::pair<std::string, Attribute*>(attr_tmp.column_name, &attr_tmp));
		if (attr_tmp.is_primary_key) meta.primary_keys.push_back(attr_tmp.id);
	}
	return meta;
}

std::string CatalogManager::attr2str(Attribute attr) {
	std::string str = "";
	str = itos(attr.id) + " " + itos(attr.type) + " " + attr.column_name + " " + itos(attr.file) + " " + itos(attr.first_page) + " " + itos(attr.first_index) + " " + itos(attr.max_length);
	str = str + " " + +itos(attr.nullable) + " " + itos(attr.is_primary_key);
	return str;
}

Attribute CatalogManager::str2attr(std::string str) {
	Attribute attr;
	std::string::size_type pos1, pos2;
	pos2 = str.find(" ");
	pos1 = 0;
	attr.id = stoi(str.substr(pos1, pos2 - pos1));
	pos1 = pos2 + 1;
	pos2 = s.find(" ", pos1);
	attr.type = stoi(str.substr(pos1, pos2 - pos1));
	pos1 = pos2 + 1;
	pos2 = s.find(" ", pos1);
	attr.column_name = str.substr(pos1, pos2 - pos1);
	pos1 = pos2 + 1;
	pos2 = s.find(" ", pos1);
	attr.file = stoi(str.substr(pos1, pos2 - pos1));
	pos1 = pos2 + 1;
	pos2 = s.find(" ", pos1);
	attr.first_page = stoi(str.substr(pos1, pos2 - pos1));
	pos1 = pos2 + 1;
	pos2 = s.find(" ", pos1);
	attr.first_index = stoi(str.substr(pos1, pos2 - pos1));
	pos1 = pos2 + 1;
	pos2 = s.find(" ", pos1);
	attr.max_length = stoi(str.substr(pos1, pos2 - pos1));
	pos1 = pos2 + 1;
	pos2 = s.find(" ", pos1);
	attr.nullable = stoi(str.substr(pos1, pos2 - pos1));
	pos1 = pos2 + 1;
	pos2 = s.find(" ", pos1);
	attr.is_primary_key = stoi(str.substr(pos1, pos2 - pos1));
	return attr;
}

std::string CatalogManager::itos(int num) {
	std::string s = "";
	if (num < 0) {
		s += "-";
		num = -num;
	}
	int digit = 0;
	int tmp = num;
	while (tmp) {
		tmp /= 10;
		digit++;
	}
	for (int i = digit; i > 0; i--) 
		s =s + (num /(int)pow(10, i - 1) % 10 + '0');
	return s;
}

int CatalogManager::stoi(std::string str) {
	return atoi(str.c_str());
}*/
