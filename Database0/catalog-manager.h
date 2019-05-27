#pragma once
#include "attribute.h"

struct ColumnName {
	const std::string& db_name;
	const std::string& table_name;
	const std::string& column_name;
};


class CatalogManager {
public:
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

	void SerializeOneTable(BufferManager::Iterator<char*>& itr, const MetaData& meta) {
		itr.AutoInsert(meta.db_name); // 如果这一页数据不够了，auto insert 能自动开一页
		itr.AutoInsert(meta.table_name);
		itr.AutoInsert(&meta.file, sizeof(FileId));

		size_t num_attribs = meta.attributes.size();

		itr.AutoInsert(&num_attribs, sizeof(num_attribs));
		for (auto& attrib : meta.attributes) {
			itr.AutoInsert(attrib.first); //map 先存key，再存value
			SerializeOneAttrib(itr, attrib.second);
		}
		
		// 注释掉为了测试用
		// size_t num_primkeys = meta.primary_keys.size();
		// itr.AutoInsert(&num_primkeys, sizeof(num_primkeys));
		// for (auto pk : meta.primary_keys) {
		// 	itr.AutoInsert(&pk, sizeof(pk));
		// }
	}

	void SerializeOneAttrib(BufferManager::Iterator<char*>& itr, const Attribute& attrib) {
		itr.AutoInsert(&attrib.id, 2 * sizeof(int)); // 这里偷懒直接把两个int塞进去了
		// itr.AutoInsert(attrib.column_name); 这里不需要，因为map的key就是colname
		itr.AutoInsert(attrib.comment);
			
		// ....
	}


	void DeserializeMeta(BufferManager::Iterator<char*>& itr, MetaData& meta) {
		meta.db_name = itr.Read<std::string>();
		meta.table_name = itr.Read<std::string>();

		meta.file = itr.Read<FileId>(); 

		size_t num_attribs = itr.Read<size_t>();

		for (size_t i = 0; i < num_attribs; ++i) {
			std::string col_name = itr.Read<std::string>();
			DeserializeOneAttribute(itr, meta.attributes[col_name], col_name);
		}

		// ...

	}

	void DeserializeOneAttribute(BufferManager::Iterator<char*>& itr, Attribute& attrib, const std::string& column_name) {
		int* data = reinterpret_cast<int*>(*itr);
		attrib.id = *data;
		attrib.type = *(++data);

		// 吃掉上面两个int
		itr.Read<int64_t>();

		attrib.column_name = column_name;
		attrib.comment = itr.Read<std::string>();

		// ...
	}
};