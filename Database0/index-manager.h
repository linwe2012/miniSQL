#pragma once
#include "attribute.h"

/////////////////////////////////////
////// Index Manager            /////
/////////////////////////////////////
class IndexManager {
public:


	// http://www.cplusplus.com/reference/cstring/memcmp/ <-- memcmp
	//无论是 int/double 都可以利用 memcpy((char*) rhs, (char*) lhs, key_size_by_bytes) 比较
	//注意 IndexManager 能从 Cat Manager 知道Key是不是边长的，这时候应该用strcmp
	BufferManager::Iterator<char> LookUp(Attribute&,
		const char* key,
		int key_size_by_bytes);
	bool IsIndexed(Attribute&);

	void BuildIndex(Attribute&);

	void DropIndex(Attribute&);

	void Insert(ItemPayload payload);

	void Delete(ItemPayload payload);

	void Update(ItemPayload payload);

};