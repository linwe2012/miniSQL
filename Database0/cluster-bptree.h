#pragma once
// clustered indexing
#include "buffer-manager.h"

template <typename Key>
class ClusteredBPTree {
public:
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

	PageId BuildIndex(BufferManager::Iterator<Key> first) {
		root_ = bm_->AllocatePageAfter(file_, 0, {Page::kfIsIndexPage});
		auto itr = bm_->GetPage<char*>(file_, root_);
		
		do
		{
			Internal intern(*first, first.page_id());
			
			itr.Insert(
				&intern,
				sizeof(intern)
			);

			first.MoveToPageEnd();
			++first; //jump to next page
		} while (!first.IsEnd());
		return root_;
	}

	void Compact();

	void Remove(BufferManager::Iterator<Key>);

private:

	PageId root_;
	FileId file_;
	BufferManager* bm_;
};


template<typename Key>
inline PageId ClusteredBPTree<Key>::Insert(const Key& key) {
	std::vector<PageId> stack;
	auto itr = bm_->GetPage<char*>(file_, root_);
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

	auto res = itr.Cast<Key>();
	res.GreaterOrEqualBinSearchIndex(key);

	if (!res.IsEndPage()  && *res == key) {
		throw std::invalid_argument("duplicated primary key");
	}

	if (res.FreeSlots() == 0) {
		int direction = res.MoveToPageCenter();
		PageId new_page = res.SplitPage();
		// move to next page if the data is at next page 
		// as is indicated by direction
		if (direction < 0) {
			++res; 
		}

		res.GreaterOrEqualBinSearchIndex(key);

		stack.pop_back(); // pop out leaf page

		bool flag_overflow = true;

		Internal reused(key, new_page);

		auto SplitAndInsert = [&reused, &itr, &direction, &new_page](Internal& intern) {
			direction = itr.MoveToPageCenter();

			new_page = itr.SplitPage();

			reused = (itr + 1).As<Internal>(); // we get first item of next page

			if (direction < 0) {
				++itr;
			}

			itr.LessOrEqualBinSearchIndex(intern);
			itr.Insert(&intern, sizeof(intern));

			reused.page_id = new_page;
		};

		while (flag_overflow && stack.size())
		{
			Internal intern(reused);

			itr = bm_->GetPage<char*>(file_, stack.back());
			stack.pop_back();

			// we still have space
			if (itr.Insert(&intern, sizeof(intern))) {
				flag_overflow = false;
				itr.LessOrEqualBinSearchIndex(intern);
				break;
			}

			SplitAndInsert(intern);
		}

		if (flag_overflow) {
			itr = bm_->GetPage<char*>(file_, root_);
			if (!itr.Insert(&reused, sizeof(reused))) {
				Internal intern(reused);
				SplitAndInsert(intern);

				root_ = bm_->AllocatePageAfter(file_, 0, { Page::kfIsIndexPage });

				itr = bm_->GetPage<char*>(file_, root_);
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

	while (!itr.GetFlagOfPage(Page::kfIsIndexPage))
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

