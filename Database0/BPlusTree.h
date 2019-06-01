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
	vector<IteratorPosition> positions;

    TreeNode* nextLeafNode; // point to the next leaf node
    
    bool isLeaf; // the flag whether this node is a leaf node
    
private:
    int degree;
    
public:
    //create a new node. if the newLeaf = false, create a branch node.Otherwise, create a leaf node
    TreeNode(int degree,bool newLeaf=false);
    
public:
	void insert(KeyType key, IteratorPosition pos);
	IteratorPosition find(KeyType key);
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
	
	void insert(KeyType key, IteratorPosition pos) {
		root->insert(key, pos);
	}

	auto find(KeyType key) {
		return root->find(key);
	}
	
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


template<typename KeyType>
void TreeNode<KeyType>::insert(KeyType key, IteratorPosition pos)
{
	size_t index;
	bool exist = search(key, index);

	if (!isLeaf) {
		return childs[index]->insert(key, pos);
	}

	if (IsFull()) {
		split(); // `split` just split the tree in half without do any insertion!!!
		search(key, index);
	}

	keys.insert(keys.begin() + index, key);
	positions.insert(positions.begin() + index, pos);
	++count;
}

template<typename KeyType>
IteratorPosition TreeNode<KeyType>::find(KeyType key)
{
	size_t index;
	bool exist = search(key, index);

	if (!isLeaf) {
		return childs[index]->find(key);
	}

	
	if (!exist) {
		return IteratorPosition();
	}

	return positions[index];
}

