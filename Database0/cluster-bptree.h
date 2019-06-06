#pragma once
// clustered indexing
#include "buffer-manager.h"
#include <tuple>

template <typename Key>
class ClusteredBPTree {
public:
	using KeyType = Key;

	ClusteredBPTree(BufferManager* bm, FileId file, PageId root)
		:root_(root), file_(file), bm_(bm) {}

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

	PageId Insert(const Key& key);

	BufferManager::Iterator<Key> Lookup(const Key& key);

	PageId BuildIndex(BufferManager::Iterator<Key> first);

	void Compact();

	void Remove(const Key& key);

	
private:
	void StackedTraverseDown(const Key& key, std::vector<PageId>* _stack, BufferManager::Iterator<char*>* _itr);


	PageId first_;
	PageId root_;
	FileId file_;
	BufferManager* bm_;
};

template<>
PageId ClusteredBPTree<char*>::Insert(const ClusteredBPTree<char*>::KeyType& key);



template<typename Key>
inline PageId ClusteredBPTree<Key>::Insert(const Key& key) {
	if (root_.IsNil()) {
		throw std::logic_error("Page is not indexed!");
	}

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

	auto res = itr.Cast<Key>();
	res.GreaterOrEqualBinSearchIndex(key);
	
	if (!res.IsEndPage()  && res.As<Key>() == key) {
		throw std::invalid_argument("duplicated primary key");
	}

	if (res.IsBeginPage() && res.As<Key>() < key) {
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

		Internal reused = Internal::MakeDummy(*res);

		PageId new_page = res.SplitPage();

		reused.page_id = new_page;

		// move to next page if the data is at next page 
		// as is indicated by direction
		if (direction < 0) {
			++res; 
		}

		res.GreaterOrEqualBinSearchIndex(key);

		stack.pop_back(); // pop out leaf page

		bool flag_overflow = true;

		auto SplitAndInsert = [&reused, &itr](const Internal& intern) {
			int direction = itr.MoveToPageCenter();

			reused = itr.As<Internal>(); // we get first item of next page

			PageId new_page = itr.SplitPage({Page::kfIsIndexPage});

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
			if (itr.FreeSlots<Internal>() != 0) {
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
	res.Insert(key);
	return root_;
}

template<typename Key>
inline BufferManager::Iterator<Key> ClusteredBPTree<Key>::Lookup(const Key& key) {
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

	auto res = itr.Cast<Key>();
	res.LessOrEqualBinSearchIndex(key);
	return res;
}

template<typename Key>
inline PageId ClusteredBPTree<Key>::BuildIndex(BufferManager::Iterator<Key> first) {
	root_ = bm_->AllocatePageAfter(file_, 0, { Page::kfIsIndexPage });
	auto itr = bm_->GetPage<char*>(file_, root_);
	if (itr.IsEnd()) {
		first_ = first.page_id();
	}


	while (!first.IsEnd())
	{
		Internal intern(*first, first.page_id());

		itr.Insert(
			&intern,
			sizeof(intern)
		);

		first.MoveToPageEnd();
		++first; //jump to next page
	}


	return root_;
}

template<typename Key>
inline void ClusteredBPTree<Key>::Remove(const Key& key) {
	std::vector<PageId> stack;
	auto itr = bm_->GetPage<char*>(file_, root_);
	StackedTraverseDown(key, &stack, &itr);

	auto res = itr.Cast<Key>();
	res.GreaterOrEqualBinSearchIndex(key);

	if (*res != key) {
		throw std::invalid_argument("deleting non exist key");
	}

	if (res.IsBeginPage()) {
		auto next = res;
		++next;
		stack.pop_back(); // pop leaf page

		itr = bm_->GetPage<char*>(file_, stack.back());
		stack.pop_back();
		itr.GreaterOrEqualBinSearchIndex(intern);

	}

	res.EraseInPage(1);

	if (res.IsPageEmpty()) {
		res.DeletePage();
	}
	
}

template<typename Key>
inline void ClusteredBPTree<Key>::StackedTraverseDown(const Key& key, std::vector<PageId>* _stack, BufferManager::Iterator<char*>* _itr) {
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

