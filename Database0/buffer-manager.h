#pragma once
// #define _HAS_CXX17 1
#include <string>
#include <string_view>
#include <map>
#include <experimental/filesystem>
#include <queue>
#include <limits>

namespace fs = std::experimental::filesystem;

template<typename UnderlyingType, int StrongType>
struct GenericIOId {
	using IdType = UnderlyingType;
	GenericIOId(UnderlyingType t) : id(t) {}
	GenericIOId() : id(0) {}

	enum : UnderlyingType {
		kMax = std::numeric_limits<UnderlyingType>::max()
	};

	operator UnderlyingType() {
		return id;
	}

	UnderlyingType id;
	bool operator<(const GenericIOId& rhs) const {
		return id < rhs.id;
	}

	bool operator==(const GenericIOId& rhs) const {
		return id == rhs.id;
	}

	bool operator!=(const GenericIOId& rhs) const {
		return !operator==(rhs);
	}

	bool operator>(const GenericIOId& rhs) const {
		return  id > rhs.id;
	}

	void Inc() {
		++id;
	}
};


using PageId = GenericIOId<uint32_t, 1>;
using FileId = GenericIOId<uint16_t, 0>;


#define PAGE_SIZE_BY_BYTES 16 * 1024

struct Variadic {
	char placeholder;
};

struct Page {
	struct Header {
		int prev = 0; // OFFSET of previous page
		int next = 0;
		uint16_t num_records = 0;
		uint16_t flags = 0;
		uint16_t free_offset = 0;
		uint64_t log_sqn = 0;     // log sequence number
		void set_flag(int i) {
			flags |= 1 << i;
		}

		void clear_flag(int i) {
			flags &= ~(1 << i);
		}

		bool flag(int i) {
			return flags & (1 << i);
		}
	};


	enum {
		kPageSize = PAGE_SIZE_BY_BYTES,
		kDiscretionSpace = kPageSize - sizeof(Header),
		kInValidOffset = UINT16_MAX,
		kBeginOfReversed = kDiscretionSpace - 1,

		kfIsVariadic = 0, // denotes this page is variadic
		kfExtendToNext = 1, // whether the last data extend
		kfIsUnused = 2
	};

	Header header;
	struct DataPos {
		uint16_t length;
		uint16_t offset;
	};

	static_assert(alignof(Header) == alignof(uint64_t), "Header Must be aligned to max so that it can match any data's align requirement");

	template<typename T>
	void Write(size_t offset, T data) {
		*(static_cast<T*>(space) + offset) = data;
	}

	template<>
	void Write<bool>(size_t offset, bool data) {
		size_t char_of_bit = offset / 8;
		size_t offset_in_char = offset % 8;
		char& target = space[char_of_bit];
		unsigned char mask = 1 << offset_in_char;

		if (data) {
			target |= mask;
		}
		else {
			target &= ~mask;
		}
	}
	/*
	void ReverseWriteBit(int offset, bool data) {
		int char_of_bit = offset/ 8 + 1;
		int offset_in_char = 7 - offset % 8;
		char& target = end()[-char_of_bit];
		unsigned char mask = 1 << offset_in_char;
		if (data) {
			target |= mask;
		}
		else {
			target &= ~mask;
		}
	}

	bool ReverseReadBit(int offset) {
		int char_of_bit = offset / 8 + 1;
		int offset_in_char = 7 - offset % 8;
		char target = end()[-char_of_bit];
		unsigned char mask = 1 << offset_in_char;
		return target & ~mask;
	}*/
	

	template<typename T>
	void ReverseInsertN(size_t src_offet, size_t num_vals, size_t move_size, T val) {
		auto t = reinterpret_cast<T*>(end());
		size_t dst_offset = num_vals + src_offet;
		memmove(t - src_offet - move_size, t - dst_offset - move_size, move_size * sizeof(T));
		
		std::fill_n(t - dst_offset, num_vals, val);
	}

	template<typename T>
	T Read(size_t offset) const {
		return *(reinterpret_cast<T*>(space) + offset);
	}

	template<typename T>
	T& Read(size_t offset) {
		return *(reinterpret_cast<T*>(space) + offset);
	}

	template<typename T>
	T& ReverseRead(size_t offset) {
		return *(reinterpret_cast<T*>(end()) - offset - 1);
	}

	template<typename T>
	T ReverseRead(size_t offset) const {
		return *(reinterpret_cast<T*>(end()) - offset - 1);
	}

	template<>
	bool Read<bool>(size_t offset) const {
		size_t char_of_bit = offset / 8;
		size_t offset_in_char = offset % 8;
		char target = space[char_of_bit];
		unsigned char mask = 1 << offset_in_char;
		return target & ~mask;
	}

	size_t SpaceLeftByByte() {
		char* begin_of_free = space + kBeginOfReversed - header.free_offset;
		if (header.flag(kfIsVariadic)) {
			char* end_of_free = space + header.num_records * sizeof(DataPos);
			return begin_of_free - end_of_free;
		}
		else {
			char* end_of_free = space + (header.num_records + 7) / 8;
			return begin_of_free - end_of_free;
		}
	}

	size_t SpaceLeftByByteFixedSize() const {
		const char* begin_of_free = space + kDiscretionSpace - header.free_offset;
		// num_records bits of null table, we round it up to char
		const char* end_of_free = space + (header.num_records + 7) / 8;
		return begin_of_free - end_of_free;
	}

	size_t SpaceLeftByByteVariadicSize() const {
		const char* begin_of_free = space + kDiscretionSpace - header.free_offset;
		// num_records bits of null table, we round it up to char
		const char* end_of_free = space + header.num_records  * sizeof(DataPos);
		return begin_of_free - end_of_free;
	}

	bool HasPrev() const {
		return header.prev != 0;
	}

	bool HasNext() const {
		return header.next != 0;
	}

	char* end() {
		return space + kDiscretionSpace;
	}

	
	char space[kDiscretionSpace];

	std::vector<bool> nil_table;
	// extra info not written to disk
	bool is_dirty = false;
	void* piggyback = nullptr;
};



class BufferManager {
public:
	template<typename T>
	class Iterator {
	public:
		using pointer = T *;
		Iterator(T* current, Page* page, BufferManager* boss)
			:current_(current), page_(page), boss_(boss), record_in_page_(0) {}

		Iterator(T* current, Page* page, BufferManager* boss, uint16_t record_in_page)
			:current_(current), page_(page), boss_(boss), record_in_page_(record_in_page) {}

		bool IsNil() const {
			return page_->ReverseRead<char>(record_in_page_);
		}

		bool IsEnd() const {
			return (record_in_page_ == page_->header.num_records)  // we reach end of the page
				&& !page_->HasNext(); // and page does not have any nexts
		}

		bool IsBegin() const {
			return (record_in_page_ == 0)  // we reach end of the page
				&& !page_->HasPrev(); // and page does not have any prevs
		}

		PageId InsertPageAfter() {
			return boss_->IteratorInsertPageAfter<T>(this);
		}

		Iterator& operator++(); // prefix

		[[deprecated("Please use prefix version as this is less efficient")]]
		Iterator operator++(int); // postfix

		Iterator& operator--();

		[[deprecated("Please use prefix version as this is less efficient")]]
		Iterator operator--(int);

		const Iterator& operator+=(int offset);

		const Iterator& operator-=(int offset);

		//TODO(L) Assert
		T& operator*() { return *current_; }

		T* operator->() { return current_; }

		const T& operator*() const { return *current_; }

		const T* operator->() const { return current_; }

		const Page& page() const { return *page_; }

		// free slots on current page, i.e. how many more T can be put into page
		size_t FreeSlots() const {
			return page_->SpaceLeftByByteFixedSize() / (size_val + sizeof(char));
		}

		size_t FreeBytes() const {
			return page_->SpaceLeftByByteFixedSize();
		}

		// returns false if space is not enough
		bool Insert(const T* first, const T* last) {
			uint16_t num_elem = static_cast<uint16_t>(last - first);
			if (num_elem > FreeSlots()) {
				return false;
			}

			size_t n_moved = page_->header.num_records - record_in_page_;
			page_->header.free_offset += static_cast<uint16_t>(num_elem * size_val);
			memmove(current_ + num_elem, current_, n_moved * size_val);
			memmove(current_, first, num_elem * size_val);

			page_->ReverseInsertN<char>(record_in_page_, num_elem, n_moved, (char)0);
			page_->header.num_records += num_elem;

			page_->is_dirty = true;
			return true;
		}

		bool Insert(T val) {
			return Insert(&val, &val + 1);
		}

		bool InsertNil(int n) {
			if (FreeBytes() < n) {
				return false;
			}
			size_t n_moved = page_->header.num_records - record_in_page_;
			page_->ReverseInsertN<char>(record_in_page_, n, n_moved, (char)1);
			page_->header.num_records += n;
			page_->is_dirty = true;
			return true;
		}

		//TODO(L): assert if uncastable (i.e. misaligned data)
		template<typename U>
		Iterator<U> Cast(Iterator t) {
			return Iterator<U>(reinterpret_cast<U*>(current_), page_, boss_, record_in_page_);
		}

	private:
		friend class BufferManager;
		T* current_;
		Page* page_;
		BufferManager* boss_;
		uint16_t record_in_page_;
		size_t size_val = sizeof(T);
	};

	// variadic parameter
	template<>
	class Iterator<char*> {
	public:
		using pointer = char*;
		Iterator(char* current, Page* page, BufferManager* boss)
			:current_(current), page_(page), boss_(boss), record_in_page_(0) {}

		Iterator(char* current, Page* page, BufferManager* boss, uint16_t record_in_page)
			:current_(current), page_(page), boss_(boss), record_in_page_(record_in_page) {}

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

		const Page& page() const { return *page_; }

		std::string AsString() {
			return std::string(current_, GetDataPos().length);
		}

		// actually this insert one data
		bool Insert(const char* first, const char* last) {
			uint16_t num_elem = static_cast<uint16_t>(last - first);
			uint16_t rounded = RoundUpByte(num_elem);
			if (num_elem > FreeBytes()) {
				return false;
			}
			size_t size_moved = page_->header.free_offset - (current_ - page_->space);

			page_->header.free_offset += rounded;
			memmove(current_ + rounded, current_, size_moved);
			memmove(current_, first, num_elem);
			

			Page::DataPos pos;

			pos.offset = static_cast<uint16_t>(current_ - page_->space);
			pos.length = num_elem;
			page_->ReverseInsertN<Page::DataPos>(record_in_page_, 1, page_->header.num_records - record_in_page_, pos);

			Page::DataPos last_pos;
			Iterator self(*this);
			++page_->header.num_records;
			for (++self; !self.IsEndPage(); ++self) {
				last_pos = self.GetDataPos();
				last_pos.offset += rounded;

				self.GetDataPos() = pos;

				pos = last_pos;
			}

			page_->is_dirty = true;
			return true;
		}

		bool Insert(const std::string& str) {
			return Insert(str.c_str(), str.c_str() + str.size());
		}

		bool Insert(void* data, size_t length_by_bytes) {
			char* cdata = reinterpret_cast<char*>(data);
			return Insert(cdata, cdata + length_by_bytes);
		}

		size_t FreeBytes() const {
			return page_->SpaceLeftByByteVariadicSize();
		}

		template<typename T>
		T RoundUpByte(T byte) {
			return (byte + 7) / 8 * 8;
		}

		bool InsertNil(int n) {
			if (FreeBytes() / sizeof(Page::DataPos) < n) {
				return false;
			}
			size_t n_moved = page_->header.num_records - record_in_page_;
			Page::DataPos pos;
			pos.offset = Page::kInValidOffset;
			pos.length = 0;
			page_->ReverseInsertN<Page::DataPos>(record_in_page_, n, n_moved, pos);
			page_->header.num_records += n;
			page_->is_dirty = true;
			return true;
		}

	private:
		// these are used for others
		/*Iterator(Variadic* current, Page* page, BufferManager* boss)
			:current_((char*)current), page_(page), boss_(boss), record_in_page_(0) {}

		Iterator(Variadic* current, Page* page, BufferManager* boss, uint16_t record_in_page)
			:current_((char*)current), page_(page), boss_(boss), record_in_page_(record_in_page) {}*/

		const Page::DataPos& GetDataPos() const { return page_->ReverseRead<Page::DataPos>(record_in_page_); }
		Page::DataPos& GetDataPos() { return page_->ReverseRead<Page::DataPos>(record_in_page_); }

		friend class BufferManager;
		char* current_;
		Page* page_;
		BufferManager* boss_;
		uint16_t record_in_page_;
	};

	struct UniquePage {
		PageId page;
		FileId file;
		mutable uint16_t use_count;
		bool operator<(const UniquePage& rhs) const { 
			if (file < rhs.file) {
				return true;
			}

			if (file > rhs.file) {
				return false;
			}

			return page < rhs.page;
		}
	};

	struct FileHeader {
		char magic[4] = "HMG";
		uint64_t num_pages = 0;
		PageId first_free = 0;
	};

	struct FileInfo {
		FILE* fd; //< file descriptor
		FileId id;
		std::string abs_path;
		FileHeader header;
	};

	struct PagePiggyback {
		FileInfo finfo;
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




	//////////////////////////////////////////////////////////////////////////////////////
	//// This part is used mostly as internal apis, ignore if ur not familiar w/ it  /////
	//////////////////////////////////////////////////////////////////////////////////////

	// allocate a new page on file
	PageId AllocatePageAfter(FileId fid, PageId prev);

	template<typename T>
	void IteratorNextPage(Iterator<T> * target);

	template<typename T>
	void IteratorPrevPage(Iterator<T> * target);

	template<typename T>
	PageId IteratorInsertPageAfter(Iterator<T>* target);

	// first loop it up in memory, if failed, find in disk
	Page* AutoFetchPage(UniquePage);

	void FlushPageToDisk(Page* page, PageId pid);

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
	
	int max_pages_ = 32;
	int num_allocate_disk_page_ = 4;

	Page* empty_page_;
};

template<typename T>
inline BufferManager::Iterator<T>& BufferManager::Iterator<T>::operator++() {

	if (record_in_page_ < page_->header.num_records) {
		if (!IsNil()) {
			++current_;
		}
		++record_in_page_;
		return *this;
	}

	// reach the end of linked list
	if (!page_->HasNext()) {
		return *this;
	}

	boss_->IteratorNextPage(this);
	return *this;
}


inline BufferManager::Iterator<char*>& BufferManager::Iterator<char*>::operator++() {

	if (record_in_page_ < page_->header.num_records) {
		++record_in_page_;
		if (IsEndPage()) {
			auto length = page_->ReverseRead<Page::DataPos>(record_in_page_ - 1).length;
			current_ += RoundUpByte(length);
			return *this;
		}
		if (!IsNil()) {
			current_ = page_->space + page_->ReverseRead<Page::DataPos>(record_in_page_).offset;
		}
		
		return *this;
	}

	// reach the end of linked list
	if (!page_->HasNext()) {
		return *this;
	}

	boss_->IteratorNextPage(this);
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
		--current_;
	}
}


inline BufferManager::Iterator<char*>& BufferManager::Iterator<char*>::operator--()
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
		current_ = page_->space + page_->ReverseRead<Page::DataPos>(record_in_page_).offset;
	}
}


template<typename T>
inline BufferManager::Iterator<T> BufferManager::Iterator<T>::operator--(int)
{
	Iterator self = *this;
	--* this;
	return self;
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
			record_in_page_ = page_->header.num_records;
			return *this;
		}

		cpage.id += page_->header.next;
		*this = boss_->GetPage<T>(piggy->finfo.id, cpage);

		last_offset = offset;
		
		offset -= page_->header.num_records;
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

	if (offset < record_in_page_) {
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
			return *this;
		}

		cpage.id += page_->header.prev;
		*this = boss_->GetPage<T>(piggy->finfo.id, cpage);

		last_offset = offset;
		offset -= num_left;

		num_left = page_->header.num_records;
	}

	return operator-=(last_offset);
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

	(*target) = GetPage<T>(piggy->finfo.id, target_page);
}

template<typename T>
inline void BufferManager::IteratorPrevPage(Iterator<T>* target)
{
	auto& page = target->page();
	auto piggy = static_cast<const PagePiggyback*>(page.piggyback);
	PageId target_page = piggy->page_id;

	target_page.id += page.header.prev;
	(*target) = GetPage<T>(piggy->finfo.id, target_page);
}

template<typename T>
inline PageId BufferManager::IteratorInsertPageAfter(Iterator<T>* target)
{
	auto piggy = static_cast<PagePiggyback*>(target->page().piggyback);
	FileId fid = piggy->finfo.id;
	return AllocatePageAfter(fid, piggy->page_id);
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
