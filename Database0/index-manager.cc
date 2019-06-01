#include "index-manager.h"

using namespace std;

void IndexManager::BuildIndex(Attribute& a)
{
	BufferManager bm;

	//����������ѽ���������Ϊ���룬�Ͳ�Ϊ�����Խ������� 
	if (IsIndexed(a) == true || a.is_primary_key == false)
	{
		throw std::invalid_argument("No need to build index on primary key");
	}
	else
	{
		// IsIndexed = true;
		if (a.type == SQLTypeID<SQLBigInt>::value)
		{
			//��һ��int���͵�B+�� 
			BPlusTree<SQLBigInt::CType>* tree = new BPlusTree<SQLBigInt::CType>(Attribute&, degree);
			auto itr = bm.GetPage<int>(a.file, a.first_page);
			while (!itr.IsEnd()) {
				//Insert()
				insertKey(itr, a.first_page);
				++itr;
			}
		}
		else if (a.type == SQLTypeID<SQLDouble>::value)
		{
			BPlusTree<SQLDouble::CType>* tree = new BPlusTree<SQLDouble::CType>(a, degree);
			auto itr = bm.GetPage<float>(a.file, a.first_page);
			while (!itr.IsEnd()) {
				//Insert()
				insertKey(itr, Attribute.first_page);
				++itr;
			}
		}
		else if(a.type == SQLTypeID<SQLString>::value)
		{
			BPlusTree<string>* tree = new BPlusTree<string>(Attribute&, degree);
			auto itr = bm.GetPage<float>(Attribute.file, Attribute.first_page);
			while (!itr.IsEnd()) {
				//Insert()
				insertKey(itr, Attribute.first_page);
				++itr;
			}
		}
	}
}

void IndexManager::DropIndex(Attribute&)
{

}

void IndexManager::Insert(ItemPayload payload)
{

}

void IndexManager::Delete(ItemPayload payload)
{

}

void IndexManager::Update(ItemPayload payload)
{

}
