#pragma once
// #define _HAS_CXX17 1
#include <memory.h>
#include <map>
#include <string>
#include <queue>
#include <limits>
#include <exception>
#include <sstream>
#include <experimental/filesystem>
#include "page.h"

struct IteratorPosition {
	PageId page_id = 0;
	Page::Offset offset = Page::kInValidOffset;
	uint16_t record_in_page;
	bool IsNil() {
		return offset == Page::kInValidOffset;
	}
};

struct IteratorSnapshot {
	uint16_t record_in_page;
	uint16_t step;
	PageId page_id;
	FileId file_id;
	Page::Offset offset = Page::kInValidOffset;
	uint64_t row;
};

class BufferManager {
public:
	template<typename T>
	class Iterator {
	public:
		using pointer = T *;
		Iterator(T* current, Page* page_id, BufferManager* boss)
			:current_(current), page_(page_id), boss_(boss), record_in_page_(0) {}

		Iterator(T* current, Page* page_id, BufferManager* boss, uint16_t record_in_page, uint64_t row)
			:current_(current), page_(page_id), boss_(boss), record_in_page_(record_in_page), row_(row){}
		
		Iterator() : Iterator(nullptr, nullptr, nullptr) {}

		bool IsValid() const {
			return page_ != nullptr;
		}

		/** 
		* check current position is nil, `operator *` returns invalid data when is nil
		*/
		bool IsNil() const {
			return page_->ReverseRead<char>(record_in_page_);
		}

		/** 
		* `operator *` returns invalid data when is end of page,
		* use `operator++` to forward to next page
		*/
		bool IsEndPage() const {
			return (record_in_page_ == page_->header.num_records);
		}

		/** 
		* `operator *` returns invalid data when is end of page
		*/
		bool IsEnd() const {
			return IsEndPage()  // we reach end of the page
				&& !page_->HasNext(); // and page does not have any nexts
		}

		/** 
		* check is beginning to linked page
		*/
		bool IsBegin() const {
			return (record_in_page_ == 0)  // we reach end of the page
				&& !page_->HasPrev(); // and page does not have any prevs
		}

		bool IsBeginPage() const {
			return  (record_in_page_ == 0);
		}

		/** 
		* insert page after the page iterator is pointing to,
		* @return new page id just inserted
		* @note iterator won't move it's postition
		*/
		PageId InsertPageAfter();

		/** 
		* forward iterator,
		* jump into next page if `IsEndPage() = true`
		* won't move if `IsEnd() = true`
		*/
		Iterator& operator++(); // prefix

		[[deprecated("Please use prefix version as this is less efficient")]]
		Iterator operator++(int); // postfix

		/** 
		* move back iterator
		*/
		Iterator& operator--();

		[[deprecated("Please use prefix version as this is less efficient")]]
		Iterator operator--(int);

		/** 
		* forward iterator by offset,
		* will walk through several pages if neccessary
		*/
		const Iterator& operator+=(int offset);

		/** 
		* move back iterator by offset,
		* will walk through several pages if neccessary
		*/
		const Iterator& operator-=(int offset);

		Iterator operator+(int offset) const;

		Iterator operator+(int offset);

		const Iterator operator-(int offset) const;

		//TODO(L) Assert
		/** 
		* Return data pointing to
		* data is invalid when `IsEndPage()` or `IsEnd()` or `IsNil()`
		*/
		T& operator*() { /*if (IsEndPage()) { ++* this; }*/  return *current_; }

		T* operator->() { return current_; }
		
		const T& operator*() const { return *current_; }
		
		const T* operator->() const { return current_; }

		/** 
		* Get page the iterator is pointing to
		*/
		const Page& page() const { return *page_; }

		/** 
		* free slots on current page, 
		* i.e. how many more element can be put into current page
		*/
		size_t FreeSlots() const {
			return page_->SpaceLeftByByteFixedSize() / (sizeof(T) * step_ + sizeof(char));
		}

		size_t ExtraSpacePerSlot() const {
			return sizeof(char);
		}

		/** 
		* free bytes of current page
		*/
		size_t FreeBytes() const {
			return page_->SpaceLeftByByteFixedSize();
		}

		/** the page id iterator is pointing to
		*/
		PageId page_id() const;

		FileId file_id() const;

		/** 
		* insert data
		* @return false if space is not enough, no data will be inserted!
		* @return true if successful
		* @param[in] first the pointing to begining of data
		* @param[in] last last of data
		* @code
		* Iterator<int> itr; int data[10]
		* itr.Insert(data, data + 10); // insert 10 element
		* 
		* int a;
		* itr.Insert(&a, &a + 1); // insert 1 element
		* @endcode
		*/
		bool Insert(const T* first, const T* last) {
			uint16_t num_elem = static_cast<uint16_t>(last - first);
			if (num_elem > FreeSlots()) {
				return false;
			}

			uint16_t defacto_num_elem = num_elem / step_;

			size_t n_moved = page_->header.num_records - record_in_page_;
			page_->header.free_offset += static_cast<uint16_t>(num_elem * sizeof(T));
			memmove(current_ + num_elem, current_, n_moved * step_ * sizeof(T));
			memmove(current_, first, num_elem * sizeof(T));

			page_->ReverseInsertN<char>(record_in_page_, defacto_num_elem, n_moved, (char)0);
			page_->header.num_records += defacto_num_elem;

			page_->is_dirty = true;
			return true;
		}

		/** @return false if space is not enough, no data will be inserted!
		* @return true if successful
		* @note calls `Insert(&val, &val + 1)`
		* @see `bool Insert(const T* first, const T* last)`
		*/
		bool Insert(T val) {
			return Insert(&val, &val + 1);
		}

		/** erase elemnts in current page, starting from position iterator is pointing to
		* @param[in] n number of elements wish to be erased
		* @return false if `n` exceeds the rest number of records in page
		*/
		bool EraseInPage(size_t n = 1) {
			uint16_t num_elem = n * step_;
			int32_t n_moved = page_->header.num_records - record_in_page_ - num_elem;
			if (n_moved <= 0) {
				return false;
			}
			page_->header.free_offset -= num_elem * sizeof(T);
			memmove(current_, current_ + num_elem, n_moved * step_ * sizeof(T));
			
			page_->ReverseEraseN<T>(record_in_page_, n, n_moved);
			page_->header.num_records -= n;
			return true;
		}

		/**
		* Delete the page itr is pointing to
		* @note the iterator will be invalid after page is deleted
		*/
		void DeletePage();


		/** 
		* Use iterator to insert data, data can span across multiple pages
		* @return false if insertion is failed, no data will be modified
		* @see `bool Insert(const T* first, const T* last)`
		*/
		bool Insert(Iterator first, Iterator last) {
			Iterator old_first = first;
			auto slots = FreeSlots();
			uint16_t count = 0;
			while (first.page_id() != last.page_id()) {
				Iterator first_endpage = first;
				first_endpage.MoveToPageEnd();
				count += first_endpage.current_ - first.current_;
				if (count > slots) {
					return false;
				}
				first = ++first_endpage;
			}
			count += last.current_ - first.current_;
			if (count > slots) {
				return false;
			}

			first = old_first;
			while (first.page_id() != last.page_id()) {
				Iterator first_endpage = first;
				first_endpage.MoveToPageEnd();
				auto res = Insert(first.current_, first_endpage.current_);
				if (!res) {
					return false;
				}
				first = ++first_endpage;
			}
			return Insert(first.current_, last.current_);
		}


		/**
		* move cursor to page center,
		* useful when trying to split a page in half,
		* @return < 0 if move backward
		* @return > 0 if move forward
		* @return 0 if not moved
		* 
		* @code
		* Iterator<int> itr;
		* itr.MoveToPageCenter();
		* auto new_page = itr.SplitPage();
		* @endcode
		*/
		int MoveToPageCenter() {
			int target = record_in_page_ - page_->header.num_records / 2;
			int res = -target;
			while (target > 0) {
				--* this;
				--target;
			}
			while (target < 0) {
				++* this;
				++target;
			}
			return res;
		}

		// if there is all data is non nil
		int MoveToPageCenterNonNil() {
			int direction = page_->header.num_records / 2 - record_in_page_;
			IgnoreNilMarchInPage(direction);
			return direction;
		}

		/**
		* move cursor to the end of the page
		*/
		void MoveToPageEnd() {
			row_ += page_->header.num_records - record_in_page_;
			record_in_page_ = page_->header.num_records;
			current_ = reinterpret_cast<T*>(page_->space + page_->header.free_offset);
		}

		/**
		* move cursor to the end of the page
		*/
		void MoveToPageBegin() {
			row_ -= record_in_page_;
			record_in_page_ = 0;
			current_ = reinterpret_cast<T*>(page_->space);
		}

		/** populate a new page and tranfer all data to next page
		* @note all data starting from curser will be copied to next page
		* and the cursur pointing to end of page i.e. cursor is not moved
		* @see MoveToPageCenter() for example
		*/
		PageId SplitPage() {
			auto piggy = reinterpret_cast<PagePiggyback*>(page_->piggyback);
			
			PageId pid = boss_->AllocatePageAfter(piggy->finfo->id, piggy->page_id);
			Iterator next = boss_->GetPage<T>(piggy->finfo->id, pid);
			next.step_ = step_;
			int16_t moved_size = static_cast<uint16_t>((page_->header.num_records - record_in_page_) * step_);
			next.Insert(current_, current_ + moved_size);
			page_->header.num_records = record_in_page_;
			page_->header.free_offset -= moved_size;
			page_->is_dirty = true;
			return pid;
		}

		/**
		* insert n nil into page
		*/
		bool InsertNil(uint16_t n) {
			if (FreeBytes() < n) {
				return false;
			}
			size_t n_moved = page_->header.num_records - record_in_page_;
			page_->ReverseInsertN<char>(record_in_page_, n, n_moved, (char)1);
			page_->header.num_records += n;
			page_->is_dirty = true;
			return true;
		}

		void SetStep(uint16_t step) {
			step_ = step;
		}

		IteratorPosition TellPosition();

		bool SearchInPage(T val) {
			while (val > *current_ && !IsBeginPage()) {
				--* this;
			}

			while (val < *current_ && !IsEndPage()) {
				++* this;
			}
			return val == *current_;
		}

		//TODO(L): assert if uncastable (i.e. misaligned data)
		/**
		* enforce cast into another type of pointer, 
		* @note use it only when it `IsBegin() = true`
		*/
		template<typename U>
		Iterator<U> Cast();

		uint64_t row() const { return row_; }

		uint16_t num_records() const;

		uint16_t record_in_page() const { return record_in_page_; }

		T* PeekNext() {
			return (current_ + step_);
		}

		

		IteratorSnapshot Snapshot() {
			return IteratorSnapshot{
				record_in_page_,
				step_,
				page_id(),
				file_id(),
				(char*)current_ - page_->space,
				row_
			};
		}

		void Restore(IteratorSnapshot snap) {
			*this = boss_->GetPage<T>(snap.file_id, snap.page_id);
			current_ = reinterpret_cast<T*>(page_->space + snap.offset);
			row_ = snap.row;
			record_in_page_ = snap.record_in_page;
			step_ = snap.step;
		}

		// locate index greater or equal to val
		// data must be Sorted && Non Nil (primary index)
		void GreaterOrEqualBinSearchIndex(const T& val) {
			int max = num_records();
			int min = 0;
			if (IsEndPage()) {
				if (IsBeginPage()) {
					return;
				}
				--* this;
			}
			while (max != min) {
				if (IsEndPage()) {
					break;
				}
				if (*(T*)current_ < val) {
					min = record_in_page_;
					int inc = (max - record_in_page_) / 2 + 1;
					IgnoreNilMarchInPage(inc);
				}
				else if (val == *(T*)current_) {
					break;
				}
				else {
					max = record_in_page_;
					int dec = (record_in_page_ - min) / 2;
					if (dec == 0) {
						if (!IsBeginPage()) {
							IgnoreNilMarchInPage(-1);
							if (*(T*)current_ == val) {
								break;
							}
							IgnoreNilMarchInPage(1);
						}
						break;
					}

					IgnoreNilMarchInPage(-dec);
				}
			}
		}

		void LessOrEqualBinSearchIndex(const T& val) {
			int max = num_records();
			int min = 0;
			if (IsEndPage()) {
				if (IsBeginPage()) {
					return;
				}
				--* this;
			}
			while (max != min) {
				if (IsEndPage()) {
					break;
				}
				if (*(T*)current_ < val) {
					min = record_in_page_;
					int inc = (max - record_in_page_) / 2;
					if (inc == 0) {
						IgnoreNilMarchInPage(1);
						if (!IsEndPage()  && *(T*)current_ == val) {
							break;
						}
						IgnoreNilMarchInPage(-1);
						break;
					}
					IgnoreNilMarchInPage(inc);
				}
				else if (val == *(T*)current_) {
					break;
				}
				else {
					max = record_in_page_;
					int dec = (record_in_page_ - min) / 2 + 1;

					IgnoreNilMarchInPage(-dec);
				}
			}
		}

		template<typename U>
		U& As() {
			return *(U*)(current_);
		}
		
		template<typename U>
		const U& As() const {
			return *(U*)(current_);
		}

		bool IsPageEmpty() {
			return page().header.num_records == 0;
		}
	private:
#pragma warning (push)
#pragma warning (disable: 4244)
		void IgnoreNilMarchInPage(int offset) {
			current_ += (int64_t)offset * step_;
			record_in_page_ += offset;
			row_ += offset;
		}
#pragma warning (pop)

		const Iterator Snapshot() const {
			return Iterator(*this);
		}

		friend class BufferManager;
		T* current_;
		Page* page_;
		BufferManager* boss_;
		uint16_t record_in_page_;
		uint16_t step_ = 1;
		uint64_t row_ = 0;
	};

	

	struct UniquePage {
		PageId page_id;
		FileId file;
		mutable uint16_t use_count;
		bool operator<(const UniquePage& rhs) const { 
			if (file < rhs.file) {
				return true;
			}

			if (file > rhs.file) {
				return false;
			}

			return page_id < rhs.page_id;
		}
	};

	struct FileHeader {
		char magic[4] = "HMG";
		uint64_t num_pages = 0;
		PageId first_free = 0;
	};

	struct FileInfo {
		void* fd; //< file descriptor
		FileId id;
		std::string abs_path;
		FileHeader header;
	};

	struct PagePiggyback {
		FileInfo *finfo;
		PageId page_id;
	};

	bool IsOpened(const char* path);

	// @throws std::ios_base::failure if path not exits
	FileId OpenFile(const char* path);

	// create a formatted file on dist, throws error when already exits
	// @throws std::ios_base::failure
	FileId NewFile(const char* path);

	// Get page from iterator
	template<typename T>
	Iterator<T> GetPage(FileId file_id, PageId page_id);


	template<typename T>
	Iterator<T> GetPage(FileId file_id, IteratorPosition pos) {
		auto itr = GetPage(file_id, pos.page_id);
		itr.MoveTo(pos);
		return itr;
	}

	//////////////////////////////////////////////////////////////////////////////////////
	//// This part is used mostly as internal apis, ignore if ur not familiar w/ it  /////
	//////////////////////////////////////////////////////////////////////////////////////

	// allocate a new page on file
	PageId AllocatePageAfter(FileId fid, PageId prev, std::vector<int> flags = std::vector<int>());

	template<typename T>
	void IteratorNextPage(Iterator<T> * target);

	template<typename T>
	void IteratorPrevPage(Iterator<T> * target);

	template<typename T>
	PageId IteratorInsertPageAfter(Iterator<T>* target);

	void IteratorDeletePage(Page* target);

	// first loop it up in memory, if failed, find in disk
	Page* AutoFetchPage(UniquePage);

	void FlushPageToDisk(Page* page_id, PageId pid);

	void FlushFileHeaderToDisk(FileId fid);

	void UnloadPage(UniquePage);

	Page* GetEmptyPage(int prev, int next);

	Page::Header GetPageHeader(FileId fid, PageId psid);

	~BufferManager();

private:
	void DeletePage(Page*);

	std::map<fs::path, FileId> loaded_files_;
	std::vector<FileInfo> file_infos_;

	std::map<UniquePage, Page*> pages_;
	
	int max_pages_ = 128;
	int num_allocate_disk_page_ = 4;

	Page* empty_page_ = nullptr;
};


// variadic parameter
template<>
class BufferManager::Iterator<char*> {
public:
	using pointer = char*;
	Iterator(char* current, Page* page_id, BufferManager* boss)
		:current_(current), page_(page_id), boss_(boss), record_in_page_(0) {}

	Iterator(char* current, Page* page_id, BufferManager* boss, uint16_t record_in_page)
		:current_(current), page_(page_id), boss_(boss), record_in_page_(record_in_page) {}

	Iterator(char* current, Page* page_id, BufferManager* boss, uint16_t record_in_page, size_t row)
		:current_(current), page_(page_id), boss_(boss), record_in_page_(record_in_page), row_(row) {}
	bool IsNil() const {
		return GetDataPos().offset == Page::kInValidOffset;
	}

	bool IsEnd() const {
		return IsEndPage()  // we reach end of the page
			&& !page_->HasNext(); // and page does not have any nexts
	}

	bool IsEndPage() const {
		return (record_in_page_ == page_->header.num_records);
	}

	bool IsBeginPage() const {
		return (record_in_page_ == 0);
	}

	bool IsBegin() const {
		return (record_in_page_ == 0)  // we reach end of the page
			&& !page_->HasPrev(); // and page does not have any prevs
	}

	//TODO(L) Assert
	char* operator*() { return current_; }

	const char* operator*() const { return current_; }

	Iterator& operator++(); // prefix

	[[deprecated("Please use prefix version as this is less efficient")]]
	Iterator operator++(int); // postfix

	Iterator& operator--();

	[[deprecated("Please use prefix version as this is less efficient")]]
	Iterator operator--(int);

	// use template version
	const Iterator& operator+=(int offset);

	// use template version
	const Iterator& operator-=(int offset);

	Iterator operator+(int offset) const;
										  
	Iterator operator-(int offset) const;


	const Page& page() const { return *page_; }

	std::string AsString() const {
		return std::string(current_, GetDataPos().length);
	}

	// actually this insert one data
	bool Insert(const char* first, const char* last);

	bool Insert(const std::string& str) {
		return Insert(str.c_str(), str.c_str() + str.size());
	}

	bool Insert(const void* data, size_t length_by_bytes) {
		const char* cdata = reinterpret_cast<const char*>(data);
		return Insert(cdata, cdata + length_by_bytes);
	}

	size_t FreeBytes() const {
		return page_->SpaceLeftByByteVariadicSize();
	}

	
	size_t FreeSlots() const {
		return FreeBytes() / (sizeof(char*) + sizeof(Page::DataPos));
	}

	size_t ExtraSpacePerSlot() const {
		return sizeof(Page::DataPos);
	}

	template<typename T>
	constexpr T RoundUpByte(T byte) {
		return (byte + 7) / 8 * 8;
	}

	bool InsertNil(int n);

	void ClearInPage() {
		page_->header.num_records = 0;
		page_->header.free_offset = 0;
		current_ = page_->space;
		record_in_page_ = 0;
		row_ = 0;
	}

	/**
	* if data exceeds free space in current page,
	* will allocate a new page and insert it
	* the cursor will atomatically forward
	* @exception when data is larger than one page can hold
	* @return `PageId(0)` (nil) if no page is allocated
	* @return `PageId(next_page)` if data is written to next page
	*/
	PageId AutoInsert(const char* first, const char* last);
	
	PageId AutoInsert(const std::string& str) {
		return AutoInsert(str.c_str(), str.c_str() + str.size() + 1);
	}

	int MoveToPageCenter();

	bool EraseInPage(size_t n = 1);

	void DeletePage();

	PageId AutoInsert(const void* data, size_t length_by_bytes) {
		const char* cdata = reinterpret_cast<const char*>(data);
		return AutoInsert(cdata, cdata + length_by_bytes);
	}

	void SkipEndPageNext() {
		++* this;
		if (IsEndPage()) {
			++* this;
		}
	}

	template<typename T>
	T Read() {
		if (GetDataPos().length != sizeof(T)) {
			// size_t len = GetDataPos().length;
			// size_t sizeT = sizeof(T);
			throw std::invalid_argument("size of the data mismatch what is written in");
		}
		T res =  *reinterpret_cast<T*>(current_);
		SkipEndPageNext();
		return res;
	}

	template<typename T>
	T& GetUnchecked() {
		return *reinterpret_cast<T*>(current_);
	}

	PageId SplitPage(std::vector<int> flags = std::vector<int>());

	std::string Read();

	PageId page_id() const;
	FileId file_id() const;

	IteratorPosition TellPosition();

	void MoveTo(IteratorPosition ip);

	void MoveToPageEnd() {
		row_ += page_->header.num_records - record_in_page_;
		record_in_page_ = page_->header.num_records;
		current_ = page_->space + page_->header.free_offset;
	}

	template<typename U>
	Iterator<U> Cast() {
		return Iterator<U>(reinterpret_cast<U*>(current_), page_, boss_, record_in_page_, row_);
	}

	template<typename T>
	void GreaterOrEqualBinSearchIndex(const T& val) {
		int max = num_records();
		int min = 0;
		if (IsEndPage()) {
			if (IsBeginPage()) {
				return;
			}
			--* this;
		}
		while (max != min) {
			if (IsEndPage()) {
				break;
			}
			if (As<T>() < val) {
				min = record_in_page_;
				int inc = (max - record_in_page_) / 2 + 1;
				*this += inc;
			}
			else if (val == As<T>()) {
				break;
			}
			else {
				max = record_in_page_;
				int dec = (record_in_page_ - min) / 2;
				if (dec == 0) {
					if (!IsBeginPage()) {
						--* this;
						if (*(T*)current_ == val) {
							break;
						}
						++* this;
					}
					
					break;
				}
				
				*this -= dec;
			}
		}
	}

	template<typename T>
	void LessOrEqualBinSearchIndex(const T& val) {
		int max = num_records();
		int min = 0;
		if (IsEndPage()) {
			if (IsBeginPage()) {
				return;
			}
			--* this;
		}
		while (max != min) {
			if (IsEndPage()) {
				break;
			}
			if (As<T>() < val) {
				min = record_in_page_;
				int inc = (max - record_in_page_) / 2;
				if (inc == 0) {
					++* this;
					if (!IsEndPage() && As<T>() == val) {
						break;
					}
					--* this;
					break;
				}
				*this += inc;
			}
			else if (val == As<T>()) {
				break;
			}
			else {
				max = record_in_page_;
				int dec = (record_in_page_ - min) / 2 + 1;

				*this -= dec;
			}
		}
	}

	uint16_t num_records() const;

	template<typename T>
	T* PeekNext() {
		return (T*)(current_ + RoundUpByte(GetDataPos(record_in_page_).length));
	}

	void SetFlagOfPage(int f) {
		page_->header.set_flag(f);
	}
	bool GetFlagOfPage(int f) {
		return page_->header.flag(f);
	}

	IteratorSnapshot Snapshot();

	void Restore(IteratorSnapshot snap);

	template<typename T>
	T& As() {
		return *reinterpret_cast<T*>(current_);
	}

	
	
	template<typename T>
	T As() const {
		return *reinterpret_cast<T*>(current_);
	}
	template<>
	std::string As<std::string>() const {
		return std::string(current_, GetDataPos().length);
	}

	/*
	const std::string As<char*>() const {
		return AsString();
	}*/

	bool IsPageEmpty() const {
		return page().header.num_records == 0;
	}

private:
	const Page::DataPos& GetDataPos() const { return page_->ReverseRead<Page::DataPos>(record_in_page_); }
	Page::DataPos& GetDataPos() { return page_->ReverseRead<Page::DataPos>(record_in_page_); }
	Page::DataPos& GetDataPos(size_t num) { return page_->ReverseRead<Page::DataPos>(num); }

	friend class BufferManager;
	char* current_;
	Page* page_;
	BufferManager* boss_;
	uint16_t record_in_page_;
	uint64_t row_ = 0;
};

template<>
inline std::string BufferManager::Iterator<char*>::Read() {
	std::string str(current_, GetDataPos().length);
	SkipEndPageNext();
	return str;
}

template<typename T>
inline PageId BufferManager::Iterator<T>::InsertPageAfter() {
	return boss_->IteratorInsertPageAfter<T>(this);
}

template<typename T>
inline BufferManager::Iterator<T>& BufferManager::Iterator<T>::operator++() {

	if (record_in_page_ < page_->header.num_records) {
		if (!IsNil()) {
			current_ += step_;
		}
		++record_in_page_;
		if (record_in_page_ != page_->header.num_records) {
			++row_;
		}
		return *this;
	}

	// reach the end of linked list
	if (!page_->HasNext()) {
		return *this;
	}

	auto step = step_;
	auto row = row_;

	boss_->IteratorNextPage(this);
	step_ = step;
	row_ = row;
	++row_;
	return *this;
}


template<typename T>
inline BufferManager::Iterator<T> BufferManager::Iterator<T>::operator++(int)
{
	Iterator self = *this;
	++* this;
	return self;
}

template<typename T>
inline BufferManager::Iterator<T>& BufferManager::Iterator<T>::operator--()
{
	if (record_in_page_ == 0) {
		// reach the end of linked list
		if (!page_->HasPrev()) {
			return *this;
		}
		boss_->IteratorPrevPage(this);
	}

	--record_in_page_;
	if (!IsNil()) {
		current_ -= step_;
	}
	--row_;
}


template<typename T>
inline BufferManager::Iterator<T> BufferManager::Iterator<T>::operator--(int)
{
	Iterator self = *this;
	--* this;
	return self;
}

template<typename T>
IteratorPosition BufferManager::Iterator<T>::TellPosition() {
	return IteratorPosition{
	page_id(),
	static_cast<Page::Offset>((char*)current_ - page_->space),
	record_in_page_,
	};
}


template<typename T>
uint16_t BufferManager::Iterator<T>::num_records() const {
	return page_->header.num_records;
}

template<typename T>
inline const BufferManager::Iterator<T>& BufferManager::Iterator<T>::operator+=(int offset)
{
	if (offset < 0) {
		return operator-=(-offset);
	}

	size_t num_left = page_->header.num_records - record_in_page_;

	if (offset <= num_left) {
		for (; offset > 0; --offset) {
			++* this;
		}
		return *this;
	}
	
	offset -= num_left;
	int last_offset = offset;

	while (offset > 0) {
		auto piggy = reinterpret_cast<PagePiggyback*>(page_->piggyback);
		PageId cpage = piggy->page_id + page_->header.next;

		if (!page_->HasNext()) {
			current_ = reinterpret_cast<pointer>(page_->header.free_offset + page_->space);
			row_ += page_->header.num_records - record_in_page_;
			record_in_page_ = page_->header.num_records;
			return *this;
		}

		auto step = step_;
		auto row = row_;
		*this = boss_->GetPage<T>(piggy->finfo->id, cpage);

		last_offset = offset;
		
		row_ = row;
		step_ = step;
		offset -= page_->header.num_records;
		row_ += page_->header.num_records;
	}
	
	return operator+=(last_offset);
}


template<typename T>
inline const BufferManager::Iterator<T>& BufferManager::Iterator<T>::operator-=(int offset)
{
	if (offset < 0) {
		return operator+=(-offset);
	}

	size_t num_left = record_in_page_;

	if (offset <= record_in_page_) {
		for (; offset > 0; --offset) {
			--* this;
		}
		return *this;
	}

	offset -= num_left;
	int last_offset = offset;

	while (offset > 0)
	{
		auto piggy = reinterpret_cast<PagePiggyback*>(page_->piggyback);
		PageId cpage = piggy->page_id;

		if (!page_->HasPrev()) {
			current_ = reinterpret_cast<pointer>(page_->space);
			record_in_page_ = 0;
			row_ -= record_in_page_;
			return *this;
		}

		cpage.id += page_->header.prev;
		*this = boss_->GetPage<T>(piggy->finfo->id, cpage);

		last_offset = offset;
		offset -= num_left;

		num_left = page_->header.num_records;
		row_ -= page_->header.num_records;
	}

	return operator-=(last_offset);
}


template<typename T>
inline const BufferManager::Iterator<T> BufferManager::Iterator<T>::operator-(int offset) const {
	Iterator i(*this);
	i -= offset;
	return i;
}

template<typename T>
inline BufferManager::Iterator<T> BufferManager::Iterator<T>::operator+(int offset) const {
	Iterator i(*this);
	i += offset;
	return i;
}

template<typename T>
inline BufferManager::Iterator<T> BufferManager::Iterator<T>::operator+(int offset) {
	return const_cast<const BufferManager::Iterator<T>*>(this)->operator+(offset);
}


template<typename T>
inline void BufferManager::Iterator<T>::DeletePage() {
	boss_->IteratorDeletePage(page_);
	page_ = nullptr;
	record_in_page_ = 0;
	current_ = nullptr;
}

template<typename T>
FileId BufferManager::Iterator<T>::file_id() const
{
	auto piggy = reinterpret_cast<PagePiggyback*>(page_->piggyback);
	return piggy->finfo->id;
}

template<typename T>
inline PageId BufferManager::Iterator<T>::page_id() const
{
	auto piggy = reinterpret_cast<PagePiggyback*>(page_->piggyback);
	return piggy->page_id;
}


template<typename T>
inline BufferManager::Iterator<T> BufferManager::GetPage(FileId file_id, PageId page_id)
{
	UniquePage unipage{ page_id, file_id };
	Page* page = AutoFetchPage(unipage);
	
	return Iterator<T>(reinterpret_cast<typename Iterator<T>::pointer>(page->space), page, this);
}

template<typename T>
inline void BufferManager::IteratorNextPage(Iterator<T>* target)
{
	auto& page = target->page();
	auto piggy = static_cast<const PagePiggyback*>(page.piggyback);
	PageId target_page = piggy->page_id;

	target_page.id += page.header.next;

	(*target) = GetPage<T>(piggy->finfo->id, target_page);
}

template<typename T>
inline void BufferManager::IteratorPrevPage(Iterator<T>* target)
{
	auto& page = target->page();
	auto piggy = static_cast<const PagePiggyback*>(page.piggyback);
	PageId target_page = piggy->page_id;

	target_page.id += page.header.prev;
	(*target) = GetPage<T>(piggy->finfo->id, target_page);
}

template<typename T>
inline PageId BufferManager::IteratorInsertPageAfter(Iterator<T>* target)
{
	auto piggy = static_cast<PagePiggyback*>(target->page().piggyback);
	FileId fid = piggy->finfo->id;
	return AllocatePageAfter(fid, piggy->page_id);
}



//TODO(L): assert if uncastable (i.e. misaligned data)
/**
* enforce cast into another type of pointer,
* @note use it only when it `IsBegin() = true`
*/
template<typename T>
template<typename U>
inline BufferManager::Iterator<U> BufferManager::Iterator<T>::Cast() {
	return Iterator<U>(reinterpret_cast<typename Iterator<U>::pointer>(current_), page_, boss_, record_in_page_, row_);
}



///////////////////////////////////////////////////
/////       Meta Data & Catalog Manager     ///////
///////////////////////////////////////////////////





#if 0
// obsolete code, may come useful some time
	void ReverseShiftBits(int dst, int src, size_t num_bits) {
		int shift = ;
		int end_dst = (dst + num_bits) / 8 + 1;
		int end_src = (src + num_bits) / 8 + 1;
		int beg_src = src / 8 + 1;
		unsigned char carry = 0;
		unsigned char* tail = ((unsigned char*)end());
		for (; end_src != beg_src;) {
			unsigned char _byte = tail[-end_src];
			unsigned char old_carry = carry;
			carry = _byte << shift;
			_byte >> (8 - shift);
			tail[-end_dst] = _byte | old_carry;

			--end_src;
			--end_dst;
		}
	}
#endif

	
