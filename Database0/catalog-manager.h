#pragma once
#include "attribute.h"

struct ColumnName {
	std::string db_name;
	std::string table_name;
	std::string column_name;
};

struct ColumnInfo {
	ColumnName name;
	std::string comment;
	int max_length;
	int type;
	bool nullable;
	bool is_primary_key;
};

class CatalogManager {
public:
	struct AttributePage {
		Attribute* attribute;
		IteratorPosition position_in_file;
	};

	void Initialize(BufferManager*, FileId, PageId);

	// 各种错误比如重复命名的table反正扔一个std::exception 就好了
	// https://en.cppreference.com/w/cpp/error/exception <-- exception 的列表
	const MetaData& FetchTable(const std::string& db_name,
		                       const std::string& table_name) const;

	MetaData& FetchTable(const std::string& db_name,
		                 const std::string& table_name);

	const Attribute& FetchColumn(ColumnName col_name);

	void NewDatabase(const std::string& db_name);//塞进map

	void NewTable(const std::string& db_name,
		         const std::string& table_name,
		         std::vector<ColumnInfo> columns);//统计metadata 塞进map

	void NewColumn(ColumnInfo col_name);

	void DropColumn(ColumnName col_name);

	void DropTable(const std::string& db_name,
		           const std::string& table_name);

	int FindTable(const std::string& db_name,
				   const std::string& table_name);

	std::vector<MetaData*> ShowTables(const std::string& db_name);

private:
	void SerializeOneTable(BufferManager::Iterator<char*>& itr, const MetaData& meta) {
		itr.AutoInsert(meta.db_name); // 如果这一页数据不够了，auto insert 能自动开一页
		itr.AutoInsert(meta.table_name);
		itr.AutoInsert(&meta.file, sizeof(FileId));

		size_t num_attribs = meta.attributes.size();

		itr.AutoInsert(&num_attribs, sizeof(num_attribs));
		for (auto& attrib : meta.attributes) {
			SerializeOneAttrib(itr, attrib);
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
		itr.AutoInsert(attrib.column_name);
		itr.AutoInsert(attrib.comment);

		itr.AutoInsert(&attrib.file, sizeof(FileId));
		itr.AutoInsert(&attrib.first_page, sizeof(PageId));
		itr.AutoInsert(&attrib.first_index,sizeof(PageId));

		itr.AutoInsert(&attrib.max_length, sizeof(int));
		itr.AutoInsert(&attrib.nullable, sizeof(bool));
		itr.AutoInsert(&attrib.is_primary_key, sizeof(bool));
	}

	void DeserializeMeta(BufferManager::Iterator<char*>& itr, MetaData& meta) {
		meta.db_name = itr.Read<std::string>();
		meta.db_name = meta.db_name.substr(0, meta.db_name.length() - 1);//string读出来会多一个空格
		meta.table_name = itr.Read<std::string>();
		meta.table_name = meta.table_name.substr(0, meta.table_name.length() - 1);

		meta.file = itr.Read<FileId>();

		size_t num_attribs = itr.Read<size_t>();

		for (size_t i = 0; i < num_attribs; ++i) {
			DeserializeOneAttribute(itr, meta.attributes[i]);
			meta.attributes_map[meta.attributes[i].column_name] = &meta.attributes[i];

			if (meta.attributes[i].is_primary_key) meta.primary_keys.push_back(meta.attributes[i].id);
		}

	}

	void DeserializeOneAttribute(BufferManager::Iterator<char*>& itr, Attribute& attrib) {
		int* data = reinterpret_cast<int*>(*itr);
		attrib.id = *data;
		attrib.type = *(++data);

		// 吃掉上面两个int
		itr.Read<int64_t>();

		attrib.column_name = itr.Read<std::string>();
		attrib.column_name = attrib.column_name.substr(0,attrib.column_name.length() - 1);
		attrib.comment = itr.Read<std::string>();
		attrib.comment = attrib.comment.substr(0, attrib.comment.length() - 1);

		attrib.file = itr.Read<FileId>();
		attrib.first_page = itr.Read<PageId>();
		attrib.first_index = itr.Read<PageId>();

		attrib.max_length = itr.Read<int>();
		attrib.nullable = itr.Read<bool>();
		attrib.is_primary_key = itr.Read<bool>();

	}

	

	         // db_name           // table_name
	std::map<std::string, std::map<std::string, MetaData>> tables_;
	int latest_id_;
	FileId file_;
	PageId page_;
	BufferManager* bm;

	/*std::string meta2str(MetaData meta);
	MetaData str2meta(std::string str);
	std::string attr2str(Attribute attr);
	Attribute str2attr(std::string str);
	std::string itos(int num);
	int stoi(std::string str);*/
};



#if 0
void SerializeOneTable(BufferManager::Iterator<char*>& tb_itr,
	BufferManager::Iterator<char*>& attrib_itr,
	const MetaData& meta) {

	tb_itr.AutoInsert(meta.db_name); // 如果这一页数据不够了，auto insert 能自动开一页
	tb_itr.AutoInsert(meta.table_name);
	tb_itr.AutoInsert(&meta.file, sizeof(FileId));


	size_t num_attribs = meta.attributes.size();

	tb_itr.AutoInsert(&num_attribs, sizeof(num_attribs));
	auto origin = tb_itr;

	SerializeAttribs(attrib_itr, meta.attributes);

	for (auto& attrib : meta.attributes) {
		if (attrib.IsNil()) {
			continue;
		}

		auto& pos = attributes_[attrib->id].position_in_file;
		tb_itr.AutoInsert(&attrib->id, sizeof(attrib->id));
		tb_itr.AutoInsert(&pos, sizeof(pos));
	}


	// 注释掉为了测试用
	// size_t num_primkeys = meta.primary_keys.size();
	// itr.AutoInsert(&num_primkeys, sizeof(num_primkeys));
	// for (auto pk : meta.primary_keys) {
	// 	itr.AutoInsert(&pk, sizeof(pk));
	// }
}

void SerializeAttribs(BufferManager::Iterator<char*>& itr,
	const std::vector<Maybe<Attribute>>& attributes) {

	for (auto attrib_maybe : attributes) {
		if (attrib_maybe.IsNil()) {
			return;
		}

		const auto& attrib = *attrib_maybe;
		auto& pos = attributes_[attrib.id].position_in_file;

		if (!pos.IsNil()) {
			return;
		}

		pos = itr.TellPosition();

		itr.AutoInsert(attrib.column_name);
		itr.AutoInsert(&attrib.id, 2 * sizeof(int)); // 这里偷懒直接把两个int塞进去了
		itr.AutoInsert(attrib.comment);

		// ....
	}
}



void DeserializeOneTable(BufferManager::Iterator<char*>& tb_itr,
	BufferManager::Iterator<char*>& attrib_itr,
	BufferManager::Iterator<char*>& itr, MetaData& meta) {
	meta.db_name = tb_itr.Read<std::string>();
	meta.table_name = tb_itr.Read<std::string>();

	meta.file = tb_itr.Read<FileId>();

	size_t num_attribs = tb_itr.Read<size_t>();
	meta.attributes.resize(num_attribs);

	for (size_t i = 0; i < num_attribs; ++i) {
		DeserializeOneAttribute(tb_itr, attrib_itr, meta);
	}

	// ...

}

void DeserializeOneAttribute(BufferManager::Iterator<char*>& tb_itr,
	BufferManager::Iterator<char*>& attrib_itr,
	MetaData& meta)
{
	int id = tb_itr.Read<int>();
	auto pos = tb_itr.Read<IteratorPosition>();

	auto is_loaded = attributes_.find(id);

	// 将载入内存的 column 写入 meta
	auto insert_to_meta = [this, &is_loaded, &meta]() {
		if (is_loaded->second.attribute == nullptr) {
			return;
		}
		auto attrib_maybe = Maybe<Attribute>(&is_loaded->second.attribute);
		meta.attributes.push_back(attrib_maybe);
		meta.attributes_map[attrib_maybe->column_name] = attrib_maybe;
		if (attrib_maybe->is_primary_key) {
			meta.primary_keys.push_back(meta.attributes.size() - 1);
		}
	};

	// 如果已经被载入内存
	if (is_loaded != attributes_.end()) {
		insert_to_meta();
		return;
	}

	attrib_itr.MoveTo(pos);

	attributes_[id] = AttributePage{
		new Attribute,
		pos
	};


	auto& attrib = *attributes_[id].attribute;

	attrib.column_name = attrib_itr.Read<std::string>();

	int* data = reinterpret_cast<int*>(*attrib_itr);
	attrib.id = *data;
	attrib.type = *(++data);

	// 吃掉上面两个int
	attrib_itr.Read<int64_t>();
	attrib.comment = attrib_itr.Read<std::string>();

	//...

	is_loaded = attributes_.find(id);
	insert_to_meta();
}


Maybe<Attribute> AllocateAttribute();
#endif
