#include "cluster-bptree.h"

template<>
PageId ClusteredBPTree<char*>::Insert(const ClusteredBPTree<char*>::KeyType& key) {
	if (root_.IsNil()) {
		throw std::logic_error("Page is not indexed!");
	}

	std::vector<PageId> stack;
	auto itr = bm_->GetPage<char*>(file_, root_);
	auto dummy = Internal::MakeDummy(key);

	std::string str(key);

	if (itr.IsEndPage()) {
		if (first_.IsNil()) {
			throw std::logic_error("Page is not indexed!");
		}
		dummy.page_id = first_;

		itr.Insert(str.c_str(), str.size());
		itr.Insert(&first_, sizeof(first_));

		auto res = bm_->GetPage<char *>(file_, first_);
		res.Insert(key);

		first_ = PageId();
		return root_;
	}

	StackedTraverseDown(key, &stack, &itr);

	auto res = itr;
	res.GreaterOrEqualBinSearchIndex(str);

	if (!res.IsEndPage() && res.As<std::string>() == str) {
		throw std::invalid_argument("duplicated primary key");
	}

	if (res.IsBeginPage() && res.As<std::string>() < str) {
		for (auto i = stack.rbegin(); i != stack.rend(); ++i) {
			itr = bm_->GetPage<char*>(file_, *i);
			itr.LessOrEqualBinSearchIndex(dummy);
			
			if (key < itr.As<std::string>()) {
				itr.EraseInPage(key);

				++itr;
				itr.As<PageId>() = key;
			}
			else {
				break;
			}
		}
	}


	if (itr.FreeSlots() == 0) {
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