#pragma once

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

	bool IsNil() { return id == 0; }
};


using PageId = GenericIOId<uint32_t, 1>;
using FileId = GenericIOId<uint16_t, 0>;


#define PAGE_SIZE_BY_BYTES 16 * 1024

struct Variadic {
	char placeholder;
};


struct Page {
	using Offset = uint16_t;
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
		kfIsUnused = 2,
		kfIsInternal = 3,
		kfIsLeaf = 4,
		kfIsIndexPage = 5,
	};


	struct DataPos {
		uint16_t length;
		uint16_t offset;
	};

	static_assert(alignof(Header) == alignof(uint64_t), "Header Must be aligned to max so that it can match any data's align requirement");

	template<typename T>
	void Write(size_t offset, T data) {
		*(static_cast<T*>(space) + offset) = data;
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
		// memmove(t - src_offet - move_size, t - dst_offset - move_size, move_size * sizeof(T));
		memmove(t - dst_offset - move_size, t - src_offet - move_size, move_size * sizeof(T));
		std::fill_n(t - dst_offset, num_vals, val);
	}

	template<typename T>
	void ReverseEraseN(size_t src_offet, size_t num_vals, size_t move_size) {
		auto t = reinterpret_cast<T*>(end());
		size_t dst_offset = src_offet - num_vals;
		memmove(t - src_offet - move_size, t - dst_offset - move_size, move_size * sizeof(T));
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
		const char* begin_of_free = space + header.free_offset;
		// num_records bits of null table, we round it up to char
		const char* end_of_free = end() - (header.num_records + 7) / 8 * 8;
		return begin_of_free - end_of_free;
	}

	size_t SpaceLeftByByteVariadicSize() const {
		const char* begin_of_free = space + kDiscretionSpace - header.free_offset;
		// num_records bits of null table, we round it up to char
		const char* end_of_free = space + header.num_records * sizeof(DataPos);
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

	const char* end() const {
		return space + kDiscretionSpace;
	}

	Header header;
	char space[kDiscretionSpace];

	// extra info not written to disk
	bool is_dirty = false;
	void* piggyback = nullptr;
};

template<>
inline void Page::Write<bool>(size_t offset, bool data) {
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

template<>
inline bool Page::Read<bool>(size_t offset) const {
	size_t char_of_bit = offset / 8;
	size_t offset_in_char = offset % 8;
	char target = space[char_of_bit];
	unsigned char mask = 1 << offset_in_char;
	return target & ~mask;
}
