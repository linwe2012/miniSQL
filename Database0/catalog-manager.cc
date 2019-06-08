#include "catalog-manager.h"
#include <stdexcept>
#include <cmath>
#include <utility>
#include <cstring>


void CatalogManager::Initialize(BufferManager* buffer_manager, FileId file, PageId page) {
	bm = buffer_manager;
	file_ = file;
	page_ = page;
}

void CatalogManager::NewDatabase(const std::string& db_name) {
	std::map<std::string, std::map<std::string, MetaData>>::iterator f = tables_.find(db_name);
	if(f!=tables_.end())
		throw std::invalid_argument("exsisted database!");

	fs::path path("./database");
	path /= db_name;
	FileId file = bm->NewFile(path.string().c_str());
	tables_[db_name]; // std::map automacitally create this slot
}

void CatalogManager::NewTable(const std::string& db_name,const std::string& table_name, std::vector<ColumnInfo> columns) {

	//if exsists a homonymic table
	if (FindTable(db_name,table_name) != -1) 
		throw std :: invalid_argument("exsisted table!");

	//metadata census
	MetaData& meta = tables_[db_name][table_name];
	meta.db_name = db_name;
	meta.table_name = table_name;
	std::string file_name = "./database/" + db_name;
	if (!fs::exists(file_name))
		throw std::invalid_argument("no such database!");
	else meta.file = bm->OpenFile(file_name.c_str());

	int id = 0;
	
	std::vector<ColumnInfo>::iterator iter;
	for (auto& iter:columns) {
		Attribute attr_tmp;
		attr_tmp.id = id++;
		attr_tmp.type = iter.type;
		attr_tmp.column_name = iter.name.column_name;
		attr_tmp.comment = iter.comment;
		attr_tmp.file = meta.file;
		attr_tmp.first_page = bm->AllocatePageAfter(meta.file, 0);//属性从哪里开始分配？
		// attr_tmp.first_index = bm->AllocatePageAfter(meta.file, 1);
		attr_tmp.max_length = iter.max_length;
		attr_tmp.is_primary_key = iter.is_primary_key;
		attr_tmp.nullable = iter.nullable;
		meta.attributes.push_back(attr_tmp);
		meta.attributes_map.insert(std::pair<std::string, Attribute*>(attr_tmp.column_name, &attr_tmp));
		if (attr_tmp.is_primary_key) meta.primary_keys.push_back(id);
	}

	auto v_meta = bm->GetPage<char*>(file_, page_);
	SerializeOneTable(v_meta, meta);
}

void CatalogManager::DropTable(const std::string& db_name, const std::string& table_name) {
	if(!FindTable(db_name,table_name))
		throw std::invalid_argument("no such table!");

	auto v_meta = bm->GetPage<char*>(file_, page_);
	for (int i = 1; i < tables_.size(); i++) {
		MetaData meta_tmp;
		DeserializeMeta(v_meta, meta_tmp);
		if (db_name == meta_tmp.db_name && table_name == meta_tmp.table_name) {
		//在page里delete这个
			break;
		}
		v_meta++;
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

	auto v_meta = bm->GetPage<char*>(file_, page_);
	MetaData meta;
	for (int i = 1; i < tables_.size(); i++) {
		DeserializeMeta(v_meta, meta);
		if (db_name == meta.db_name && table_name == meta.table_name)
			return meta;
		v_meta++;
	}
}

Attribute& CatalogManager::FetchColumn(ColumnName col_name) {
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
	
	auto v_meta = bm->GetPage<char*>(file_, page_);
	for (int i = 1; i < tables_.size(); i++) {
		MetaData meta_tmp;
		DeserializeMeta(v_meta, meta_tmp);
		if (col_name.name.db_name == meta_tmp.db_name && col_name.name.table_name == meta_tmp.table_name) {
			Attribute attr_tmp;
			attr_tmp.id = meta_tmp.attributes.size()+ 1;
			attr_tmp.type = col_name.type;
			attr_tmp.column_name = col_name.name.column_name;
			attr_tmp.comment = col_name.comment;
			attr_tmp.file = meta_tmp.file;
			attr_tmp.first_page = bm->AllocatePageAfter(meta_tmp.file, 0);//属性从哪里开始分配？
			attr_tmp.max_length = col_name.max_length;
			attr_tmp.is_primary_key = col_name.is_primary_key;
			attr_tmp.nullable = col_name.nullable;
			meta_tmp.attributes.push_back(attr_tmp);
			meta_tmp.attributes_map.insert(std::pair<std::string, Attribute*>(attr_tmp.column_name, &attr_tmp));
			if (attr_tmp.is_primary_key) meta_tmp.primary_keys.push_back(attr_tmp.id);
			//drop original
			SerializeOneTable(v_meta,meta_tmp);
			break;
		}
		v_meta++;
	}


}

void CatalogManager::DropColumn(ColumnName col_name) {
	if (!FindTable(col_name.db_name, col_name.table_name))
		throw std::invalid_argument("no such table!");
	MetaData table = FetchTable(col_name.db_name, col_name.table_name);
	std::map<std::string, Attribute*>::iterator m_iter = table.attributes_map.find(col_name.column_name);
	if (m_iter == table.attributes_map.end())
		throw std::invalid_argument("no such column!");

	auto v_meta = bm->GetPage<char*>(file_, page_);
	for (int i = 1; i < tables_.size(); i++) {
		MetaData meta_tmp;
		DeserializeMeta(v_meta, meta_tmp);
		if (col_name.db_name == meta_tmp.db_name && col_name.table_name == meta_tmp.table_name) {
			std::vector<Attribute> ::iterator iter;
			for (iter = meta_tmp.attributes.begin(); iter < meta_tmp.attributes.end(); iter++) {
				if (iter->column_name == col_name.column_name)
					break;
			}
			meta_tmp.attributes.erase(iter,iter+1);
			std::string del_col(col_name.column_name);
			meta_tmp.attributes_map.erase(del_col);
			std::vector<int> ::iterator i_iter;
			for (i_iter = meta_tmp.primary_keys.begin(); i_iter < meta_tmp.primary_keys.end(); i_iter++) {
				if (*i_iter == iter->id)
					break;
			}
			if (i_iter != meta_tmp.primary_keys.end()) meta_tmp.primary_keys.erase(i_iter,i_iter + 1);
			break;
			//drop original
			SerializeOneTable(v_meta, meta_tmp);
		}
		v_meta++;
	}
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
