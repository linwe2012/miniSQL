#pragma once
#include "buffer-manager.h"

template <typename Key, typename Prim>
class BPTree {
public:
	using KeyType = Key;

	BPTree(BufferManager* bm, FileId file, PageId root)
		:root_(root), file_(file), bm_(bm) {}

	BPTree(BufferManager* bm, FileId file, PageId root, PageId first)
		:root_(root), file_(file), bm_(bm), first_(first) {}

	struct Internal {
		Internal(const Key& _key) : key(_key) {}
		Internal(const Key& _key, const PageId _page_id)
			: key(_key), page_id(_page_id) {}

		static Internal MakeDummy(const Key& _key) { return Internal{ _key }; }

		bool operator <(const Internal& rhs) const { return key < rhs.key; }
		bool operator >(const Internal& rhs) const { return key > rhs.key; }
		bool operator ==(const Internal& rhs) const { return key == rhs.key; }
		bool operator <=(const Internal& rhs) const { return key <= rhs.key; }
		bool operator >=(const Internal& rhs) const { return key >= rhs.key; }
		bool operator !=(const Internal& rhs) const { return key != rhs.key; }

		Key key;
		PageId page_id;
	};

	struct Leaf {
		Key key;
		Prim prim;

		bool operator <(const  Leaf& rhs) const { return key < rhs.key; }
		bool operator >(const  Leaf& rhs) const { return key > rhs.key; }
		bool operator ==(const Leaf& rhs) const { return key == rhs.key; }
		bool operator <=(const Leaf& rhs) const { return key <= rhs.key; }
		bool operator >=(const Leaf& rhs) const { return key >= rhs.key; }
		bool operator !=(const Leaf& rhs) const { return key != rhs.key; }
	};

	PageId Insert(const Key& key, const Prim& prim);

	BufferManager::Iterator<Leaf> Lookup(const Key& key);

	PageId BuildIndex(BufferManager::Iterator<Key> first, BufferManager::Iterator<Prim> prim);

	void Compact();

	uint64_t ComputeOffset(const Key& key, PageId first) {
		auto itr = Lookup(key);
		auto s_itr = bm_->GetPage<char*>(file_, first);
		uint64_t num_row = 0;
		while (first != itr.page_id())
		{
			num_row += s_itr.num_records();
			s_itr.MoveToPageEnd();
			++s_itr;
		}

		num_row += itr.record_in_page();
		return num_row;
	}

	void Remove(const Key& key);

	template<typename U, typename V>
	BPTree<U, V> Cast() {
		return BPTree<U, V>(
			bm_,
			file_,
			root_,
			first_
			);
	}

private:
	void StackedTraverseDown(const Key& key, std::vector<PageId>* _stack, BufferManager::Iterator<char*>* _itr);


	PageId first_;
	PageId root_;
	FileId file_;
	BufferManager* bm_;
};


template<typename Key, typename Prim>
inline PageId BPTree<Key, Prim>::Insert(const Key& key, const Prim& prim) {
	if (root_.IsNil()) {
		throw std::logic_error("Page is not indexed!");
	}

	Leaf leaf{
		key,
		prim
	};
	std::vector<PageId> stack;
	auto itr = bm_->GetPage<char*>(file_, root_);
	auto dummy = Internal::MakeDummy(key);

	if (itr.IsEndPage()) {
		if (first_.IsNil()) {
			throw std::logic_error("Page is not indexed!");
		}
		dummy.page_id = first_;

		itr.Insert(&dummy, sizeof(dummy));

		auto res = bm_->GetPage<Key>(file_, first_);
		res.Insert(key);

		first_ = PageId();
		return root_;
	}

	StackedTraverseDown(key, &stack, &itr);

	auto res = itr.Cast<Leaf>();
	res.GreaterOrEqualBinSearchIndex(leaf);

	if (!res.IsEndPage() && res.As<Leaf>() == leaf) {
		throw std::invalid_argument("duplicated primary key");
	}

	if (res.IsBeginPage() && res.As<Leaf>() < leaf) {
		for (auto i = stack.rbegin(); i != stack.rend(); ++i) {
			itr = bm_->GetPage<char*>(file_, *i);
			itr.LessOrEqualBinSearchIndex(dummy);
			if (key < itr.As<Internal>().key) {
				itr.As<Internal>().key = key;
			}
			else {
				break;
			}
		}
	}


	if (res.FreeSlots() == 0) {
		int direction = res.MoveToPageCenter();

		Internal reused = Internal::MakeDummy((*res).key);

		PageId new_page = res.SplitPage();

		reused.page_id = new_page;

		// move to next page if the data is at next page 
		// as is indicated by direction
		if (direction < 0) {
			++res;
		}

		res.GreaterOrEqualBinSearchIndex(leaf);

		stack.pop_back(); // pop out leaf page

		bool flag_overflow = true;

		auto SplitAndInsert = [&reused, &itr](const Internal& intern) {
			int direction = itr.MoveToPageCenter();

			reused = itr.As<Internal>(); // we get first item of next page

			PageId new_page = itr.SplitPage({ Page::kfIsIndexPage });

			if (direction < 0) {
				++itr;
			}

			itr.GreaterOrEqualBinSearchIndex(intern);
			itr.Insert(&intern, sizeof(intern));

			reused.page_id = new_page;
		};

		while (flag_overflow && stack.size())
		{
			Internal intern(reused);

			itr = bm_->GetPage<char*>(file_, stack.back());
			stack.pop_back();

			// we still have space
			if (itr.FreeSlots() != 0) {
				flag_overflow = false;
				itr.GreaterOrEqualBinSearchIndex(intern);
				itr.Insert(&intern, sizeof(intern));
				break;
			}

			SplitAndInsert(intern);
		}

		if (flag_overflow) {
			itr = bm_->GetPage<char*>(file_, root_);
			itr.GreaterOrEqualBinSearchIndex(reused);

			if (!itr.Insert(&reused, sizeof(reused))) {
				Internal intern(reused);
				SplitAndInsert(intern);

				PageId old_root = root_;
				root_ = bm_->AllocatePageAfter(file_, 0, { Page::kfIsIndexPage });

				itr = bm_->GetPage<char*>(file_, old_root);
				intern = itr.As<Internal>();
				intern.page_id = old_root;

				itr = bm_->GetPage<char*>(file_, root_);
				itr.Insert(&intern, sizeof(intern));
				++itr;
				itr.Insert(&reused, sizeof(reused));
			}
		}
	}

	// ++res; // move to first larger
	res.Insert(leaf);
	return root_;
}

template<typename Key, typename Prim>
inline BufferManager::Iterator<typename BPTree<Key, Prim>::Leaf> BPTree<Key, Prim>::Lookup(const Key& key) {
	auto itr = bm_->GetPage<char*>(file_, root_);
	auto dummy = Internal::MakeDummy(key);

	while (itr.GetFlagOfPage(Page::kfIsIndexPage))
	{
		itr.LessOrEqualBinSearchIndex(dummy);
		if (itr.IsEndPage()) {
			--itr;
		}

		PageId next_page = itr.GetUnchecked<Internal>().page_id;
		itr = bm_->GetPage<char*>(file_, next_page);
	}

	Leaf leaf{
		key,
		Prim()
	};
	auto res = itr.Cast<Leaf>();
	res.LessOrEqualBinSearchIndex(leaf);
	return res;
}

template<typename Key, typename Prim>
inline PageId BPTree<Key, Prim>::BuildIndex(BufferManager::Iterator<Key> first, BufferManager::Iterator<Prim> prim) {
	root_ = bm_->AllocatePageAfter(file_, 0, { Page::kfIsIndexPage });
	auto leaf_page = bm_->AllocatePageAfter(file_, 0);
	auto itr = bm_->GetPage<char*>(file_, root_);
	if (itr.IsEnd()) {
		first_ = first.page_id();
	}

	Leaf leaf{
		*first,
		*prim
	};
	Internal intern{
		*first,
		leaf_page
	};
	++first;
	++prim;
	itr.Insert(&intern, sizeof(intern));
	auto cc = bm_->GetPage<Leaf>(file_, leaf_page);
	cc.Insert(leaf);
	while (!first.IsEnd())
	{
		Insert(*first, *prim);
		++first, ++prim;
		if (first.IsEndPage()) {
			++first;
		}
		if (prim.IsEndPage()) {
			++prim;
		}
	}


	return root_;
}

template<typename Key, typename Prim>
inline void BPTree<Key, Prim>::Remove(const Key& key) {
	std::vector<PageId> stack;
	auto itr = bm_->GetPage<char*>(file_, root_);
	StackedTraverseDown(key, &stack, &itr);

	auto res = itr.Cast<Key>();
	res.GreaterOrEqualBinSearchIndex(key);

	if (*res != key) {
		throw std::invalid_argument("deleting non exist key");
	}

	bool del_page = res.num_records() == 1;

	Key next_key((res + 1).As<Key>());

	if (res.IsBeginPage()) {
		Internal intern = Internal::MakeDummy(key);
		stack.pop_back(); // pop leaf page
		bool flag_underflow = true;

		while (stack.size()) {
			itr = bm_->GetPage<char*>(file_, stack.back());
			stack.pop_back();

			itr.GreaterOrEqualBinSearchIndex(intern);
			if (!(itr.As<Key>() == key)) {
				flag_underflow = false;
				break;
			}


			itr.EraseInPage(1);
			auto next = (itr + 1);
			if (itr.IsPageEmpty()) {

			}
		}
	}

	res.EraseInPage(1);

	if (res.IsPageEmpty()) {
		res.DeletePage();
	}

}

template<typename Key, typename Prim>
inline void BPTree<Key, Prim>::StackedTraverseDown(const Key& key, std::vector<PageId>* _stack, BufferManager::Iterator<char*>* _itr) {
	auto& stack = *_stack;
	auto& itr = *_itr;
	auto dummy = Internal::MakeDummy(key);

	while (itr.GetFlagOfPage(Page::kfIsIndexPage))
	{
		itr.LessOrEqualBinSearchIndex(dummy);
		if (itr.IsEndPage()) {
			--itr;
		}

		PageId next_page = itr.GetUnchecked<Internal>().page_id;
		stack.push_back(next_page);

		itr = bm_->GetPage<char*>(file_, next_page);
	}
}
