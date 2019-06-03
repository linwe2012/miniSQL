#pragma once
#include "catalog-manager.h"
#include <stdexcept>
#include <cmath>
#include <utility>

void CatalogManager::Initialize(BufferManager* buffer_manager, FileId file, PageId page) {
	bm = *buffer_manager;
	file_ = file;
	page_ = page;
}

void CatalogManager::NewDatabase(const std::string& db_name) {
	std::map<std::string, std::map<std::string, MetaData>>::iterator f = tables_.find(db_name);
	if(f!=tables_.end())
		throw std::invalid_argument("exsisted database!");

	FileId file = bm.NewFile("./database/$dbname");
	tables_.insert(std::pair<std::string, std::map<std::string, MetaData>>(db_name, std::pair<std::string, MetaData>(NULL, NULL)));
}

void CatalogManager::NewTable(const std::string& db_name,const std::string& table_name, std::vector<ColumnInfo> columns) {

	//if exsists a homonymic table
	if (FindTable(db_name,table_name) != -1) 
		throw std :: invalid_argument("exsisted table!");

	//metadata census
	MetaData meta;
	meta.db_name = db_name;
	meta.table_name = table_name;
	if (!fs::exists("./database/$db_name"))
		throw std::invalid_argument("no such database!");
	else meta.file = bm.OpenFile("./database/$db_name");

	std::vector<ColumnInfo>::iterator iter;
	int id = 0;
	for (iter = columns.begin(); iter != columns.end(); iter++) {
		Attribute attr_tmp;
		attr_tmp.id = id++;
		attr_tmp.type = (*iter).type;
		attr_tmp.column_name = (*iter).name.column_name;
		attr_tmp.comment = (*iter).comment;
		attr_tmp.file = meta.file;
		attr_tmp.first_page = bm.AllocatePageAfter(meta.file, 1);//属性从哪里开始分配？
		attr_tmp.first_index = bm.AllocatePageAfter(meta.file, 1);
		attr_tmp.max_length = (*iter).max_length;
		attr_tmp.is_primary_key = (*iter).is_primary_key;
		attr_tmp.nullable = (*iter).nullable;
		meta.attributes.push_back(attr_tmp);
		meta.attributes_map.insert(std::pair<std::string, Attribute*>(attr_tmp.column_name, &attr_tmp));
		if (attr_tmp.is_primary_key) meta.primary_keys.push_back(id);
	}

	auto v_meta = bm.GetPage<char*>(file_, page_);
	v_meta.Insert(meta);

	tables_.insert(std::pair<std::string, std::map<std::string, MetaData>>(db_name, std::pair<std::string, MetaData>(table_name, meta)));
}

void CatalogManager::DropTable(const std::string& db_name, const std::string& table_name) {
	if(!FindTable(db_name,table_name))
		throw std::invalid_argument("no such table!");

	auto v_meta = bm.GetPage<char*>(file_, page_);
	for (int i = 1; i < tables_.size(); i++) {
		MetaData meta_tmp;
		meta_tmp = *(v_meta + i);
		if (db_name == meta_tmp.db_name && table_name == meta_tmp.table_name) {
		//在page里delete这个
			break;
		}
	}
	std::map<std::string, std::map<std::string, MetaData>>::iterator iter;
	iter = tables_.find(db_name);
	std::map<std::string, MetaData>::iterator iiter;
	iiter = iter->second.find(table_name);
	(iter->second).erase(iiter);
}

MetaData& CatalogManager::FetchTable(const std::string& db_name, const std::string& table_name) {
	if (!FindTable(db_name, table_name))
		throw std::invalid_argument("no such table!");

	auto v_meta = bm.GetPage<char*>(file_, page_);
	MetaData meta;
	for (int i = 1; i < tables_.size(); i++) {
		meta = *(v_meta + i);
		if (db_name == meta.db_name && table_name == meta.table_name)
			return meta;
	}
}

const Attribute& CatalogManager::FetchColumn(ColumnName col_name) {
	MetaData table = FetchTable(col_name.db_name, col_name.table_name);
	std::map<std::string, Attribute*>::iterator m_iter = table.attributes_map.find(col_name.column_name);
	if(m_iter == table.attributes_map.end())
		throw std::invalid_argument("no such column!");
	return *(m_iter->second);
}

int CatalogManager::FindTable(const std::string& db_name, const std::string& table_name) {
	std::map<std::string, std::map<std::string, MetaData>>::iterator iter = tables_.find(db_name);
	if (iter == tables_.end())
		return 0;
	std::map<std::string, MetaData>::iterator iiter;
	iiter = (iter->second).find(table_name);
	if (iiter == (iter->second).end())
		return 0;
	return 1;
}

void CatalogManager::NewColumn(ColumnInfo col_name) {
	if(!FindTable(col_name.name.db_name,col_name.name.table_name))
		throw std::invalid_argument("no such table!");
	MetaData table = FetchTable(col_name.name.db_name, col_name.name.table_name);
	std::map<std::string, Attribute*>::iterator m_iter = table.attributes_map.find(col_name.name.column_name);
	if (m_iter != table.attributes_map.end())
		throw std::invalid_argument("existed column!");
	
	auto v_meta = bm.GetPage<char*>(file_, page_);
	for (int i = 1; i < tables_.size(); i++) {
		MetaData meta_tmp;
		meta_tmp = *(v_meta + i);
		if (col_name.name.db_name == meta_tmp.db_name && col_name.name.table_name == meta_tmp.table_name) {
			Attribute attr_tmp;
			attr_tmp.id = meta_tmp.attributes.size()+ 1;
			attr_tmp.type = col_name.type;
			attr_tmp.column_name = col_name.name.column_name;
			attr_tmp.comment = col_name.comment;
			attr_tmp.file = meta_tmp.file;
			attr_tmp.first_page = bm.AllocatePageAfter(meta_tmp.file, 1);//属性从哪里开始分配？
			attr_tmp.first_index = bm.AllocatePageAfter(meta_tmp.file, 1);
			attr_tmp.max_length = col_name.max_length;
			attr_tmp.is_primary_key = col_name.is_primary_key;
			attr_tmp.nullable = col_name.nullable;
			(*(v_meta+i)).attributes.push_back(attr_tmp);
			(*(v_meta + i)).attributes_map.insert(std::pair<std::string, Attribute*>(attr_tmp.column_name, &attr_tmp));
			if (attr_tmp.is_primary_key) (*(v_meta + i)).primary_keys.push_back(attr_tmp.id);
			break;
		}
	}
}

void CatalogManager::DropColumn(ColumnName col_name) {
	if (!FindTable(col_name.db_name, col_name.table_name))
		throw std::invalid_argument("no such table!");
	MetaData table = FetchTable(col_name.db_name, col_name.table_name);
	std::map<std::string, Attribute*>::iterator m_iter = table.attributes_map.find(col_name.column_name);
	if (m_iter == table.attributes_map.end())
		throw std::invalid_argument("no such column!");

	auto v_meta = bm.GetPage<char*>(file_, page_);
	for (int i = 1; i < tables_.size(); i++) {
		MetaData meta_tmp;
		meta_tmp = *(v_meta + i);
		if (col_name.db_name == meta_tmp.db_name && col_name.table_name == meta_tmp.table_name) {
			std::vector<Attribute> ::iterator iter;
			for (iter = (v_meta + i)->attributes.begin(); iter < (v_meta + i)->attribute.end(); iter++) {
				if (iter->column_name == col_name.column_name)
					break;
			}
			(*(v_meta + i)).attributes.erase(iter,iter+1);
			std::map<std::string, Attribute*> m_iter = (v_meta + i)->find(col_name.column_name);
			(*(v_meta + i)).attributes_map.errase(m_iter);
			std::vector<int> ::iterator i_iter;
			for (i_iter = (v_meta + i)->primary_keys.begin(); i_iter < (v_meta + i)->primary_keys.end(); i_iter++) {
				if (*i_iter == iter->id)
					break;
			}
			if (i_iter != (v_meta + i)->primary_keys.end()) (*(v_meta + i)).primary_keys.erase(i_iter,i_iter + 1);
			break;
		}
	}
}


/*std::string CatalogManager::itos(int num) {
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
		s += (num /(int)pow(10, i - 1) % 10 + '0');
	return s;
}*/