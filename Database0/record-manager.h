#pragma once
#include "index-manager.h"

#include "catalog-manager.h"
/*
class Inserter : public ISQLDataVisitor{
public:
	void Visit(SQLBigInt* val) override {
		
	}

	IndexManager& index;
	BufferManager::I
	size_t pos;
	size_t which;
};
*/
class RecordManager {
public:
	
	void Initilize(BufferManager* bm, FileId fid, PageId pid) {
		index_.Initialize(bm, fid, pid);
	}

	void Insert(MetaData& meta, std::vector<ISQLData*> data) {
		int primary_index = meta.primary_keys[0];
		
		auto prim_raw = data[primary_index]->GetRaw();
		auto pos = index_.LookUp(meta.attributes[primary_index], reinterpret_cast<const char*>(prim_raw.ptr), prim_raw.size);
		size_t rowid = pos.row();
		
		for (int i = 0; i < data.size(); ++i) {
			auto datum = data[i];
			auto& attrib = meta.attributes[i];

			auto raw = datum->GetRaw();

			if (datum->IsVariadic()) {
				auto itr = buffer_->GetPage<char*>(attrib.file, attrib.first_page);
				// 跳转到正确的row
				itr += rowid;
				//index_.Insert(ItemPayload{ itr.Cast<char>(),
				//	reinterpret_cast<const char*>(raw.ptr),
				//	static_cast<uint64_t>(raw.size),
				//	true
				//	});
			}
			else {
				auto itr = buffer_->GetPage<char>(attrib.file, attrib.first_page);
				itr.SetStep(raw.size);
				itr += rowid;
				//index_.Insert(ItemPayload{ itr,
				//	reinterpret_cast<const char*>(raw.ptr),
				//	static_cast<uint64_t>(raw.size),
				//	true
				//	});
			}
		}
	}

private:
	IndexManager index_;
	CatalogManager catalog_;
	BufferManager* buffer_;
};