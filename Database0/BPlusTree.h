#pragma once

#include <vector>
#include <stdio.h>
#include <string.h>
#include "buffer-manager.h"

using namespace std;

typedef int offsetNumber; // the value of the tree node

template <typename KeyType>
class TreeNode{
public:
    size_t count;  // the count of keys
    TreeNode* parent;
    vector <KeyType> keys;
    vector <TreeNode*> childs;
    vector <offsetNumber> vals;
	vector<PageId> values;

    TreeNode* nextLeafNode; // point to the next leaf node
    
    bool isLeaf; // the flag whether this node is a leaf node
    
private:
    int degree;
    
public:
    //create a new node. if the newLeaf = false, create a branch node.Otherwise, create a leaf node
    TreeNode(int degree,bool newLeaf=false);
    
public:
	bool IsFull();
    bool isRoot();
    bool search(KeyType key,size_t &index);//search a key and return by the reference of a parameter
	TreeNode* split();
    TreeNode* splite(KeyType &key);
    size_t add(BufferManager& bm, FileId file, KeyType &key); //add the key in the branch and return the position
    size_t add(KeyType &key,offsetNumber val); // add a key-value in the leaf node and return the position
    bool removeAt(size_t index);
};

template <typename KeyType>
class BPlusTree
{
private:
    typedef TreeNode<KeyType>* Node;

    // a struct helping to find the node containing a specific key
    struct searchNodeParse
    {
        Node pNode; // a pointer pointering to the node containing the key
        size_t index; // the position of the key
        bool ifFound; // the flag that whether the key is found.
    };
private:
    string fileName;
    Node root;
    Node leafHead; // the head of the leaf node
    size_t keyCount;
    size_t level;
    size_t nodeCount;
//    fileNode* file; // the filenode of this tree
    int keySize; // the size of key
    int degree;
    
public:
    BPlusTree(string m_name,int keySize,int degree);
    ~BPlusTree();

	
    offsetNumber search(KeyType& key); // search the value of specific key
    bool insertKey(KeyType &key,offsetNumber val);
    bool deleteKey(KeyType &key);
    
    void dropTree(Node node);
    
    void readFromDiskAll();
    void writtenbackToDiskAll();
    void readFromDisk(blockNode* btmp);

private:
    void init_tree();// init the tree
    bool adjustAfterinsert(Node pNode);
    bool adjustAfterDelete(Node pNode);
    void findToLeaf(Node pNode,KeyType key,searchNodeParse &snp);
	FileId file_;
};

//This function is used to add the key in the branch node and return the position added.
template <class KeyType>
size_t TreeNode<KeyType>::add(BufferManager& bm,FileId file, KeyType& key)
{
	if (count == 0)
	{
		keys[0] = key;
		count++;
		return 0;
	}
	else //count > 0
	{
		size_t index = 0; // record the index of the tree
		bool exist = search(key, index);
		if (exist)
		{
			throw std::invalid_argument("Error:In add(Keytype &key),key has already in the tree!");
		}
		else // leaf node
		{
			
			auto itr = bm.GetPage<KeyType>(file, values[index]);
			itr.SearchInPage(key);

			if (itr.FreeSlots() == 0) {
				if (IsFull()) {
					split(); // splite just split the tree in half without do any insertion!!!
				}

				search(*itr, index);
				itr.MoveToPageCenter();
				PageId new_page = itr.SplitPage();
				keys.insert(keys.begin() + index, *(itr - 1));
				values.insert(values.begin() + index, new_page);
			}

			itr.Insert(key);

			++count;

				// keys.insert(keys.begin() + count, key);
				// 
				// 
				// // for(size_t i = count;i > index;i --)
				// //     keys[i] = keys[i-1];
				// // keys[index] = key;
				// 
				// for(size_t i = count + 1;i > index+1;i --)
				//     childs[i] = childs[i-1];
				// childs[index+1] = NULL; // this child will link to another node
				// count ++;

				return index;
		}
	}
}


