#include "IndexManager.h"
#include <iostream>
using namespace std;

void IndexManager::BuildIndex(Attribute&)
{
	BufferManager bm;
	
	//����������ѽ���������Ϊ���룬�Ͳ�Ϊ�����Խ������� 
	if(IsIndexed == true || Attribute.is_primary_key == false)
	{
		cout << "No need to build an index!" << endl;
	}
	else
	{
		IsIndexed = true;
		if(Attribute.type == TYPE_INT)
		{
			//��һ��int���͵�B+�� 
			BPlusTree<int> *tree = new BPlusTree<int>(Attribute&,degree);
			auto itr = bm.GetPage<int>(Attribute.file, Attribute.first_page);
			while (!itr.IsEnd()) {
				//Insert()
				insertKey(itr, Attribute.first_page);
				++itr;
			}
		}
		else if(Attribute.type == TYPE_FLOAT)
		{
			BPlusTree<float> *tree = new BPlusTree<float>(Attribute&,degree);
			auto itr = bm.GetPage<float>(Attribute.file, Attribute.first_page);
			while (!itr.IsEnd()) {
				//Insert()
				insertKey(itr, Attribute.first_page);
				++itr;
			}
		}
		else
		{
			BPlusTree<string> *tree = new BPlusTree<string>(Attribute&,degree);
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
