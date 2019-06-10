#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE
#endif // _MSC_VER

#include "buffer-manager.h"

bool BufferManager::Iterator<char*>::Insert(const char* first, const char* last) {
	uint16_t num_elem = static_cast<uint16_t>(last - first);
	uint16_t rounded = RoundUpByte(num_elem);
	if (rounded + sizeof(Page::DataPos) > FreeBytes()) {
		return false;
	}
	size_t size_moved = page_->header.free_offset - (current_ - page_->space);

	page_->header.free_offset += rounded;
	memmove(current_ + rounded, current_, size_moved);
	memmove(current_, first, num_elem);


	Page::DataPos pos;

	pos.offset = static_cast<uint16_t>(current_ - page_->space);
	pos.length = num_elem;

	// page_->ReverseInsertN<Page::DataPos>(record_in_page_, 1, page_->header.num_records - record_in_page_, pos);
	
	++page_->header.num_records;

	Page::DataPos last_pos;
	Iterator self(*this);
	
	for (; !self.IsEndPage(); ++self) {
		last_pos = self.GetDataPos();
		last_pos.offset += rounded;

		self.GetDataPos() = pos;

		pos = last_pos;
	}

	page_->is_dirty = true;
	return true;
}

#pragma warning (push)
#pragma warning(disable: 4244)
bool BufferManager::Iterator<char*>::InsertNil(int n) {
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
#pragma warning (pop)

/**
* if data exceeds free space in current page,
* will allocate a new page and insert it
* the cursor will atomatically forward
* @exception when data is larger than one page can hold
* @return `PageId(0)` (nil) if no page is allocated
* @return `PageId(next_page)` if data is written to next page
*/
PageId BufferManager::Iterator<char*>::AutoInsert(const char* first, const char* last) {
	if (!Insert(first, last)) {
		auto piggy = reinterpret_cast<PagePiggyback*>(page_->piggyback);
		PageId next = boss_->AllocatePageAfter(piggy->finfo->id, piggy->page_id);
		*this = boss_->GetPage<char*>(piggy->finfo->id, next);
		if (!Insert(first, last)) {
			throw std::overflow_error("The string is too long");
		}
		// page_->is_dirty = true;
		++* this;
		return next;
	}

	++* this;
	// page_->is_dirty = true;
	return PageId();
}

bool BufferManager::Iterator<char*>::EraseInPage(size_t n) {
	Iterator self = *this;
	if (page_->header.num_records - record_in_page_ < n) {
		return false;
	}
	uint16_t length = GetDataPos(record_in_page_ + n).offset + GetDataPos(record_in_page_ + n).length - GetDataPos().offset;
	int32_t n_moved = page_->header.free_offset - length;
	if (n_moved <= 0) {
		return false;
	}
	page_->header.free_offset -= length;
	memmove(current_, current_ + length, n_moved);

	page_->ReverseEraseN<Page::DataPos>(record_in_page_, n, n_moved);
	page_->header.num_records -= n;
	return true;
}



BufferManager::Iterator<char*>& BufferManager::Iterator<char*>::operator++() {

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
		++row_;
		return *this;
	}

	// reach the end of linked list
	if (!page_->HasNext()) {
		return *this;
	}

	boss_->IteratorNextPage(this);
	++row_;
	return *this;
}

BufferManager::Iterator<char*>& BufferManager::Iterator<char*>::operator--()
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
	--row_;
	return *this;
}

#pragma warning (push)
#pragma warning (disable: 4267 4244)
const BufferManager::Iterator<char*>& BufferManager::Iterator<char*>::operator-=(int offset)
{
	if (offset < 0) {
		return operator+=(-offset);
	}

	size_t num_left = record_in_page_;

	if (offset <= record_in_page_) {
		record_in_page_ -= offset;
		if (record_in_page_ == 0) {
			current_ = page_->space;
		}
		else {
			const auto& pos = GetDataPos(record_in_page_ - 1);
			current_ = page_->space + pos.offset + RoundUpByte(pos.length);
		}
		
		row_ -= offset;
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
			row_ -= record_in_page_;
			record_in_page_ = 0;
			return *this;
		}

		cpage.id += page_->header.prev;
		*this = boss_->GetPage<char*>(piggy->finfo->id, cpage);

		last_offset = offset;
		offset -= num_left;

		num_left = page_->header.num_records;
		row_ -= page_->header.num_records;
	}

	return operator-=(last_offset);
}


const BufferManager::Iterator<char*>& BufferManager::Iterator<char*>::operator+=(int offset)
{
	if (offset < 0) {
		return operator-=(-offset);
	}

	size_t num_left = num_records() - record_in_page_;

	if (offset <= num_left) {
		record_in_page_ += offset;
		if (IsBeginPage()) {
			current_ = page_->space;
		}
		else {
			const auto& pos = GetDataPos(record_in_page_ - 1);
			current_ = page_->space + pos.offset + RoundUpByte(pos.length);
		}
		
		row_ += offset;
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

		*this = boss_->GetPage<char*>(piggy->finfo->id, cpage);

		last_offset = offset;

		offset -= page_->header.num_records;
		row_ += page_->header.num_records;
	}

	return operator+=(last_offset);
}
#pragma warning (pop)

void BufferManager::Iterator<char*>::Restore(IteratorSnapshot snap) {
	*this = boss_->GetPage<char*>(snap.file_id, snap.page_id);
	current_ = (page_->space + snap.offset);
	row_ = snap.row;
	record_in_page_ = snap.record_in_page;
}


IteratorSnapshot BufferManager::Iterator<char*>::Snapshot() {
	return IteratorSnapshot{
		record_in_page_,
		1,
		page_id(),
		file_id(),
		static_cast<Page::Offset>((char*)current_ - page_->space),
		row_
	};
}

void BufferManager::Iterator<char*>::MoveTo(IteratorPosition ip) {
	if (ip.page_id != page_id()) {
		*this = boss_->GetPage<char*>(file_id(), page_id());
	}

	current_ = reinterpret_cast<char*>(page_->space + ip.offset);

	record_in_page_ = ip.record_in_page;
}

int BufferManager::Iterator<char*>::MoveToPageCenter() {
	int direction = (int)(num_records() / 2) - record_in_page_;
	*this += direction;
	return direction;
}


PageId BufferManager::Iterator<char*>::SplitPage(std::vector<int> flags) {
	auto piggy = reinterpret_cast<PagePiggyback*>(page_->piggyback);

	PageId pid = boss_->AllocatePageAfter(piggy->finfo->id, piggy->page_id, flags);
	Iterator next = boss_->GetPage<char *>(piggy->finfo->id, pid);

	Iterator self = *this;
	while (!self.IsEndPage()) {
		next.Insert(self.current_, self.GetDataPos().length);
		++self;
		++next;
	}

	page_->header.num_records = record_in_page_;
	page_->header.free_offset = GetDataPos().offset;
	page_->is_dirty = true;
	return pid;
}


uint16_t BufferManager::Iterator<char*>::num_records() const {
	return page_->header.num_records;
}


FileId BufferManager::Iterator<char*>::file_id() const
{
	auto piggy = reinterpret_cast<PagePiggyback*>(page_->piggyback);
	return piggy->finfo->id;
}

PageId BufferManager::Iterator<char*>::page_id() const
{
	auto piggy = reinterpret_cast<PagePiggyback*>(page_->piggyback);
	return piggy->page_id;
}

BufferManager::Iterator<char*> BufferManager::Iterator<char*>::operator-(int offset) const {
	Iterator i(*this);
	i -= offset;
	return i;
}

BufferManager::Iterator<char*> BufferManager::Iterator<char*>::operator+(int offset) const {
	Iterator i(*this);
	i += offset;
	return i;
}