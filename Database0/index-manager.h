#pragma once
#include "attribute.h"

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
		int key_size_by_bytes);

	bool IsIndexed(Attribute&);

	void BuildIndex(Attribute&);

	void DropIndex(Attribute&);

	void Insert(Attribute& a, ItemPayload payload) {

	}

	void Delete(ItemPayload payload);

	void Update(ItemPayload payload);

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