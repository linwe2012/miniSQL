#pragma once
// clustered indexing
#include "buffer-manager.h"

template <typename Key>
class ClusteredBPTree {
public:
	ClusteredBPTree(BufferManager* bm, FileId file, PageId root);
	struct Leaf {
		PageId vals[NumLeaf];
		Leaf* left;
		Leaf* right;

	};

	void Insert(const Key& key) {
		
	}

	struct Internal {
		static Internal MakeDummy(const Key& _key) : key(_key) {}

		Key key;
		PageId page_id;
		bool operator <(const Internal& rhs) const { return key < rhs.key; }
		bool operator >(const Internal& rhs) const { return key > rhs.key; }
		bool operator ==(const Internal& rhs) const { return key == rhs.key; }
		bool operator <=(const Internal& rhs) const { return key <= rhs.key; }
		bool operator >=(const Internal& rhs) const { return key >= rhs.key; }
		bool operator !=(const Internal& rhs) const { return key != rhs.key; }
	};

	BufferManager::Iterator<Key> Lookup(const Key& key) {
		auto itr = bm_->GetPage<char*>(file_, root_);
		auto dummy = Internal::MakeDummy(key);

		while (!itr.GetFlagOfPage(Page::kfIsIndexPage))
		{
			itr.BinarySearchInPageAsIndex(dummy);
			if (itr.IsEndPage()) {
				--itr;
			}

			PageId next_page = GetUnchecked<Internal>().page_id;
			itr = bm_->GetPage<char*>(file_, next_page);
		}

		auto res = itr.Cast<Key>();
		res.SearchInPage(key);
		return res;
	}

private:
	PageId root_;
	FileId file_;
	BufferManager* bm_;
};