#pragma once
#include "attribute.h"
#include "BPlusTree.h"
#include "buffer-manager.h"

/////////////////////////////////////
////// Index Manager            /////
/////////////////////////////////////
class IndexManager {
public:

	void Initialize(BufferManager*, FileId, PageId);

	// http://www.cplusplus.com/reference/cstring/memcmp/ <-- memcmp
	//������ int/double ���������� memcpy((char*) rhs, (char*) lhs, key_size_by_bytes) �Ƚ�
	//ע�� IndexManager �ܴ� Cat Manager ֪��Key�ǲ��Ǳ߳��ģ���ʱ��Ӧ����strcmp
	BufferManager::Iterator<char> LookUp(Attribute&,
		const char* key,
		int key_size_by_bytes) {
		
	}

	BufferManager::Iterator<char> LookUp(Attribute& a,
		SQLBigInt::CType key) {
		auto pos = GetBPTree<SQLBigInt::CType>(a)->find(key);
		if (pos.IsNil()) {
			return BufferManager::Iterator<char>();
		}

		return bm->GetPage<SQLBigInt::CType>(a.file, pos).Cast<char>();
	}

	bool IsIndexed(Attribute&);

	void BuildIndex(Attribute&);

	void DropIndex(Attribute&);

	void Insert(Attribute& a, ItemPayload payload) {
		
		if (!IsIndexed(a)) {
			return;
		}
		switch (a.type)
		{
		case SQLTypeID<SQLBigInt>::value:
			auto bpt = GetBPTree<SQLBigInt::CType>(a);
			bpt->insert(*reinterpret_cast<const SQLBigInt::CType*>(payload.data),  // key
				         payload.target.TellPosition());
			break;
		default:
			break;
		}
	}

	void Delete(ItemPayload payload);

	void Update(ItemPayload payload);

private:
	template<typename T>
	BPlusTree<T>* GetBPTree(Attribute& a);
	BufferManager* bm;

};


// void xxx() {
// 	Attribute a;
// 	BufferManager bm;
// 	auto itr = bm.GetPage<int>(a.file, a.first_page);
// 	while (!itr.IsEnd()) {
// 		int data = *itr;
// 
// 		//....
// 
// 		++itr;
// 
// 		PageId page = itr.SplitPage();
// 
// 
// 	}
// }