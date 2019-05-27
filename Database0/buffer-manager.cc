<<<<<<< HEAD
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE
#endif // _MSC_VER

#include "buffer-manager.h"
#include <fstream>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
/*
class FStream {
public:
	FStream(fs::path p) {
		Open(p);
	}

	FStream() {}
	FStream(const FStream&) = delete;

	const FStream& operator=(const FStream&) = delete;

	void Open(fs::path p) {
		ios.open(p, std::ios::binary | std::ios::in | std::ios::out);
		if (!ios.is_open()) {
			ios.clear();
			throw std::ios_base::failure("Unbale o open file");
		}
	}

	void NewFile(fs::path p) {
		std::ofstream os(p);
		os.close();
		ios.open(p);
	}
	void Write(const void* data, size_t size) {
		ios.write(reinterpret_cast<const char*>(data), size);
	}
	void SeekPut(size_t pos, int way) {
		ios.seekp(pos, way);
	}

	void SeekGet(size_t pos, int way) {
		ios.seekg(pos, way);
	}

	
	void Read(void* buf, size_t size) {
		ios.read(reinterpret_cast<char*>(buf), size);
	}

	void Flush() {
		ios.flush();
	}

	size_t TellGet() {
		return ios.tellg();
	}

	size_t TellPut() {
		return ios.tellp();
	}

private:
	std::fstream ios;
};*/

class FStream {
public:
	FStream(fs::path p) {
		Open(p);
	}

	FStream() {}
	FStream(const FStream&) = delete;

	const FStream& operator=(const FStream&) = delete;

	void Open(fs::path p) {
		fd = open(p.string().c_str(), O_BINARY | O_RDWR, S_IREAD | S_IWRITE);

		if (fd == -1) {
			throw std::ios_base::failure("Unbale o open file");
		}
	}

	void NewFile(fs::path p) {
		fd = open(p.string().c_str(), O_BINARY | O_CREAT | O_RDWR, S_IREAD | S_IWRITE);
		if (fd == -1) {
			throw std::ios_base::failure("Unbale o open file");
		}
	}
	void Write(const void* data, size_t size) {
		auto res = write(fd, data, size);
		if (res == -1) {
			int a = EBADF;
			int b = ENOSPC;
			int c = EINVAL;
			int err = errno;
			throw std::ios_base::failure("Unable to write");
		}
		if (res != size) {
			throw std::ios_base::failure("Not All written to file");
		}
	}

	void Read(void* buf, size_t size) {
		auto res = read(fd, buf, size);
		if (res != size) {
			throw std::ios_base::failure("Read failed");
		}
	}
	void SeekPut(size_t pos, int way) {

		auto res = lseek(fd, pos, SEEK_SET);
		if (way == std::ios::beg) {
			if (res != pos) {
				throw std::ios_base::failure("Fail to seek");
			}
		}
	}

	void SeekGet(size_t pos, int way) {
		auto res = lseek(fd, pos, SEEK_SET);
		if (way == std::ios::beg) {
			if (res != pos) {
				throw std::ios_base::failure("Fail to seek");
			}
		}
	}

	void Flush() {

	}

	size_t TellGet() {
		return tell(fd);
	}

	size_t TellPut() {
		return tell(fd);
	}

	~FStream() {
		close(fd);
	}
private:
	int fd = -1;
	size_t pos;
};

bool BufferManager::IsOpened(const char* path)
{
	fs::path canon = fs::canonical(path);
	if (loaded_files_.find(canon) == loaded_files_.end()) {
		return false;
	}
	return true;
}

FStream& GetStream(BufferManager::FileInfo& finfo) {
	return *reinterpret_cast<FStream*>(finfo.fd);
}

void TestFileOk(FILE* f, const std::string& abs_path) {
	if (f == nullptr) {
		std::string what = "Unable to open file at: ";
		what += abs_path + ". error code(";
		what += std::to_string(errno) + ")";
		throw std::ios_base::failure(what);
	}
}

// https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/read?view=vs-2019
FileId BufferManager::OpenFile(const char* path)
{
	fs::path canon = fs::canonical(path);
	auto itr = loaded_files_.find(canon);
	if (itr != loaded_files_.end()) {
		return itr->second.id;
	}

	FileInfo finfo;
	finfo.fd = new FStream(path);
	
	// finfo.fd = fopen(canon.string().c_str(), "r+");
	// finfo.fd = open(canon.string().c_str(), O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
	finfo.abs_path = canon.string();

	// TestFileOk(finfo.fd, finfo.abs_path);
	if (file_infos_.size() >= FileId::kMax) {
		throw std::overflow_error("Too many files!");
	}

	finfo.id = FileId(static_cast<FileId::IdType>(file_infos_.size()));
	loaded_files_[canon] = finfo.id;
	GetStream(finfo).Read(&finfo.header, sizeof(finfo.header));
	// fread(&finfo.header, sizeof(finfo.header), 1, finfo.fd);
	file_infos_.push_back(finfo);
	
	return finfo.id;
}

FileId BufferManager::NewFile(const char* path)
{
	FileInfo finfo;
	fs::path canon = path;
	if (fs::exists(path)) {
		std::string what = "File:" + canon.string() + " already exits";
		throw std::ios_base::failure(what);
	}
	else{
		// finfo.fd = fopen(canon.string().c_str(), "w+");
		finfo.fd = new FStream();
		canon = fs::canonical(path);
	}
	
	auto itr = loaded_files_.find(canon);
	if (itr != loaded_files_.end()) {
		return itr->second.id;
	}

	
	finfo.abs_path = canon.string();
	// TestFileOk(finfo.fd, finfo.abs_path);

	Page* page = GetEmptyPage(0, 0);

	auto& ios = GetStream(finfo);
	ios.NewFile(path);
	ios.Write(page, Page::kPageSize);
	
	// fwrite(page, Page::kPageSize, 1, finfo.fd);
	// fseek(finfo.fd, 0, SEEK_SET);
	
	if (file_infos_.size() >= FileId::kMax) {
		throw std::overflow_error("Too many files!");
	}

	finfo.id = FileId(static_cast<FileId::IdType>(file_infos_.size()));

	finfo.header.first_free = 0;
	finfo.header.num_pages = 0;

	loaded_files_[canon] = finfo.id;
	file_infos_.push_back(finfo);

	FlushFileHeaderToDisk(finfo.id);

	return finfo.id;
}

Page* BufferManager::AutoFetchPage(UniquePage unipage)
{
	auto itr = pages_.find(unipage);
	if (itr != pages_.end()) {
		++(itr->first.use_count);
		return itr->second;
	}

	
	FileInfo& finfo = file_infos_[unipage.file];
	PagePiggyback* ppb = new PagePiggyback;
	ppb->finfo = finfo;
	ppb->page_id = unipage.page;

	Page* page = new Page;
	page->piggyback = ppb;

	auto& ios = GetStream(finfo);
	ios.SeekGet(unipage.page * Page::kPageSize, std::ios::beg);
	ios.Read(page, Page::kPageSize);
	
	// auto seel_res = fseek(finfo.fd, unipage.page * Page::kPageSize, SEEK_SET);
	// lseek(finfo.fd, unipage.page * Page::kPageSize, SEEK_SET);
	/*
	auto res = fread(page, Page::kPageSize, 1, finfo.fd);
	if (res != 1) {
		std::ostringstream ss;
		ss << "Fail to read page " << unipage.page << " in file: " << finfo.abs_path;
		if (ferror(finfo.fd)) {
			ss << ". error code(" << errno << ").";
		}
		else {
			ss << ". Unexpected end of file.";
		}
		
		clearerr(finfo.fd);
		throw std::ios_base::failure(ss.str());
	}*/

	pages_[unipage] = page;
	return page;
}

void BufferManager::FlushPageToDisk(Page* page, PageId pid) {
	auto piggy = static_cast<PagePiggyback*>(page->piggyback);
	auto& ios = GetStream(piggy->finfo);
	ios.SeekPut(pid * Page::kPageSize, std::ios::beg);
	ios.Write(page, Page::kPageSize);
	ios.Flush();
	page->is_dirty = false;
}

void BufferManager::FlushFileHeaderToDisk(FileId fid)
{
	auto& finfo = file_infos_[fid];
	auto& ios = GetStream(finfo);
	ios.SeekPut(0, std::ios::beg);
	ios.Write(&finfo.header, sizeof(finfo.header));
	ios.Flush();
}

void BufferManager::UnloadPage(UniquePage unipage)
{
	auto itr = pages_.find(unipage);
	if (itr == pages_.end()) {
		return;
	}

	if (itr->second->is_dirty) {
		FlushPageToDisk(itr->second, unipage.page);
	}

	DeletePage(itr->second);

	pages_.erase(itr);
}

BufferManager::~BufferManager()
{
	for (auto itr = pages_.begin(); itr != pages_.end(); ) {
		UnloadPage((itr++)->first);
	}

	
	for (auto finfo : file_infos_) {
		delete &GetStream(finfo);
		finfo.fd = nullptr;
	}
	
	// STLs are expcted to do the rest jobs
}

PageId BufferManager::AllocatePageAfter(FileId fid, PageId prev)
{
	auto populate_adjust_page = [this, &fid, &prev](PageId current) -> Page* {
		Page* page = nullptr;
		// we are poupulate a fresh new page
		if (prev.id == 0) {
			page = GetEmptyPage(0, 0);
		}
		else {
			// get previous page
			Page* prev_page = AutoFetchPage(UniquePage{
				prev,
				fid
				});

			// update previous page's valude pointing to self
			int next = prev_page->header.next;
			prev_page->header.next = current - prev;
			prev_page->is_dirty = true;

			// get next page
			int next_offset = 0;
			if (next != 0) {
				Page* next_page = AutoFetchPage(UniquePage{
					PageId(prev + next),
					fid
					});
				next_page->header.prev = current - next;
				next_page->is_dirty = true;

				next_offset = next - current;
			}
			page = GetEmptyPage(prev - current, next_offset);
		}
		page->header.clear_flag(Page::kfIsUnused);
		return page;
	};

	auto& finfo = file_infos_[fid];
	if (finfo.header.first_free.id == 0) {
		++finfo.header.num_pages;
		if (finfo.header.num_pages > PageId::kMax) {
			throw std::overflow_error("Too many pages in file");
		}
		PageId current = static_cast<PageId::IdType>(finfo.header.num_pages);


		long end_of_file = current * Page::kPageSize;
		auto& ios = GetStream(finfo);
		
		
		auto page = populate_adjust_page(current);
		ios.SeekPut(end_of_file, std::ios::beg);
		ios.Write(page, Page::kPageSize);
		
		// auto after_write_size = ios.TellPut();
		// if (after_write_size != (current.id + 1) * Page::kPageSize) {
		// 	throw std::ios_base::failure("Something went wrong when writing to file");
		// }
		
		return current;
	}

	PageId result = finfo.header.first_free;
	Page* page = AutoFetchPage(UniquePage{
		result,
		fid
		});

	auto& header = page->header;
	if (!header.flag(Page::kfIsUnused)) {
		std::stringstream ss;
		ss << "File: " << finfo.abs_path
			<< " at page " << finfo.header.first_free
			<< " is expected to be unused, but page says it is used, please check for file corruption";
		throw std::ios_base::failure(ss.str());
	}

	Page* adjusted_page = populate_adjust_page(result);

	memcpy(&page->header, &adjusted_page->header, sizeof(header));

	if (header.next == 0) {
		finfo.header.first_free = 0;
	}
	else {
		finfo.header.first_free.id += header.next;
	}
	
	FlushPageToDisk(page, result);
	FlushFileHeaderToDisk(fid);
	return result;
}

Page* BufferManager::GetEmptyPage(int prev, int next)
{
	if (empty_page_ == nullptr) {
		empty_page_ = new Page;
		empty_page_->is_dirty = false;
		empty_page_->header.set_flag(Page::kfIsUnused);
	}
	empty_page_->header.prev = prev;
	empty_page_->header.next = next;
	return empty_page_;
}

Page::Header BufferManager::GetPageHeader(FileId fid, PageId pid)
{
	UniquePage unipage{ pid, fid };
	auto itr = pages_.find(unipage);
	if (itr != pages_.end()) {
		return itr->second->header;
	}

	Page::Header header;
	auto& ios = GetStream(file_infos_[fid]);
	ios.SeekGet(pid * Page::kPageSize, std::ios::beg);
	ios.Read(&header, sizeof(header));
	return header;
}

void BufferManager::DeletePage(Page* page)
{
	auto piggy = static_cast<PagePiggyback*>(page->piggyback);
	delete piggy;
	delete page;
}





=======
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE
#endif // _MSC_VER

#include "buffer-manager.h"
#include <fstream>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
/*
class FStream {
public:
	FStream(fs::path p) {
		Open(p);
	}

	FStream() {}
	FStream(const FStream&) = delete;

	const FStream& operator=(const FStream&) = delete;

	void Open(fs::path p) {
		ios.open(p, std::ios::binary | std::ios::in | std::ios::out);
		if (!ios.is_open()) {
			ios.clear();
			throw std::ios_base::failure("Unbale o open file");
		}
	}

	void NewFile(fs::path p) {
		std::ofstream os(p);
		os.close();
		ios.open(p);
	}
	void Write(const void* data, size_t size) {
		ios.write(reinterpret_cast<const char*>(data), size);
	}
	void SeekPut(size_t pos, int way) {
		ios.seekp(pos, way);
	}

	void SeekGet(size_t pos, int way) {
		ios.seekg(pos, way);
	}

	
	void Read(void* buf, size_t size) {
		ios.read(reinterpret_cast<char*>(buf), size);
	}

	void Flush() {
		ios.flush();
	}

	size_t TellGet() {
		return ios.tellg();
	}

	size_t TellPut() {
		return ios.tellp();
	}

private:
	std::fstream ios;
};*/

class FStream {
public:
	FStream(fs::path p) {
		Open(p);
	}

	FStream() {}
	FStream(const FStream&) = delete;

	const FStream& operator=(const FStream&) = delete;

	void Open(fs::path p) {
		fd = open(p.string().c_str(), O_BINARY | O_RDWR, S_IREAD | S_IWRITE);

		if (fd == -1) {
			throw std::ios_base::failure("Unbale o open file");
		}
	}

	void NewFile(fs::path p) {
		fd = open(p.string().c_str(), O_BINARY | O_CREAT | O_RDWR, S_IREAD | S_IWRITE);
		if (fd == -1) {
			throw std::ios_base::failure("Unbale o open file");
		}
	}
	void Write(const void* data, size_t size) {
		auto res = write(fd, data, size);
		if (res == -1) {
			int a = EBADF;
			int b = ENOSPC;
			int c = EINVAL;
			int err = errno;
			throw std::ios_base::failure("Unable to write");
		}
		if (res != size) {
			throw std::ios_base::failure("Not All written to file");
		}
	}

	void Read(void* buf, size_t size) {
		auto res = read(fd, buf, size);
		if (res != size) {
			throw std::ios_base::failure("Read failed");
		}
	}
	void SeekPut(size_t pos, int way) {

		auto res = lseek(fd, pos, SEEK_SET);
		if (way == std::ios::beg) {
			if (res != pos) {
				throw std::ios_base::failure("Fail to seek");
			}
		}
	}

	void SeekGet(size_t pos, int way) {
		auto res = lseek(fd, pos, SEEK_SET);
		if (way == std::ios::beg) {
			if (res != pos) {
				throw std::ios_base::failure("Fail to seek");
			}
		}
	}

	void Flush() {

	}

	size_t TellGet() {
		return tell(fd);
	}

	size_t TellPut() {
		return tell(fd);
	}

	~FStream() {
		close(fd);
	}
private:
	int fd = -1;
	size_t pos;
};

bool BufferManager::IsOpened(const char* path)
{
	fs::path canon = fs::canonical(path);
	if (loaded_files_.find(canon) == loaded_files_.end()) {
		return false;
	}
	return true;
}

FStream& GetStream(BufferManager::FileInfo& finfo) {
	return *reinterpret_cast<FStream*>(finfo.fd);
}

void TestFileOk(FILE* f, const std::string& abs_path) {
	if (f == nullptr) {
		std::string what = "Unable to open file at: ";
		what += abs_path + ". error code(";
		what += std::to_string(errno) + ")";
		throw std::ios_base::failure(what);
	}
}

// https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/read?view=vs-2019
FileId BufferManager::OpenFile(const char* path)
{
	fs::path canon = fs::canonical(path);
	auto itr = loaded_files_.find(canon);
	if (itr != loaded_files_.end()) {
		return itr->second.id;
	}

	FileInfo finfo;
	finfo.fd = new FStream(path);
	
	// finfo.fd = fopen(canon.string().c_str(), "r+");
	// finfo.fd = open(canon.string().c_str(), O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
	finfo.abs_path = canon.string();

	// TestFileOk(finfo.fd, finfo.abs_path);
	if (file_infos_.size() >= FileId::kMax) {
		throw std::overflow_error("Too many files!");
	}

	finfo.id = FileId(static_cast<FileId::IdType>(file_infos_.size()));
	loaded_files_[canon] = finfo.id;
	GetStream(finfo).Read(&finfo.header, sizeof(finfo.header));
	// fread(&finfo.header, sizeof(finfo.header), 1, finfo.fd);
	file_infos_.push_back(finfo);
	
	return finfo.id;
}

FileId BufferManager::NewFile(const char* path)
{
	FileInfo finfo;
	fs::path canon = path;
	if (fs::exists(path)) {
		std::string what = "File:" + canon.string() + " already exits";
		throw std::ios_base::failure(what);
	}
	else{
		// finfo.fd = fopen(canon.string().c_str(), "w+");
		finfo.fd = new FStream();
		canon = fs::canonical(path);
	}
	
	auto itr = loaded_files_.find(canon);
	if (itr != loaded_files_.end()) {
		return itr->second.id;
	}

	
	finfo.abs_path = canon.string();
	// TestFileOk(finfo.fd, finfo.abs_path);

	Page* page = GetEmptyPage(0, 0);

	auto& ios = GetStream(finfo);
	ios.NewFile(path);
	ios.Write(page, Page::kPageSize);
	
	// fwrite(page, Page::kPageSize, 1, finfo.fd);
	// fseek(finfo.fd, 0, SEEK_SET);
	
	if (file_infos_.size() >= FileId::kMax) {
		throw std::overflow_error("Too many files!");
	}

	finfo.id = FileId(static_cast<FileId::IdType>(file_infos_.size()));

	finfo.header.first_free = 0;
	finfo.header.num_pages = 0;

	loaded_files_[canon] = finfo.id;
	file_infos_.push_back(finfo);

	FlushFileHeaderToDisk(finfo.id);

	return finfo.id;
}

Page* BufferManager::AutoFetchPage(UniquePage unipage)
{
	auto itr = pages_.find(unipage);
	if (itr != pages_.end()) {
		++(itr->first.use_count);
		return itr->second;
	}

	
	FileInfo& finfo = file_infos_[unipage.file];
	PagePiggyback* ppb = new PagePiggyback;
	ppb->finfo = finfo;
	ppb->page_id = unipage.page;

	Page* page = new Page;
	page->piggyback = ppb;

	auto& ios = GetStream(finfo);
	ios.SeekGet(unipage.page * Page::kPageSize, std::ios::beg);
	ios.Read(page, Page::kPageSize);
	
	// auto seel_res = fseek(finfo.fd, unipage.page * Page::kPageSize, SEEK_SET);
	// lseek(finfo.fd, unipage.page * Page::kPageSize, SEEK_SET);
	/*
	auto res = fread(page, Page::kPageSize, 1, finfo.fd);
	if (res != 1) {
		std::ostringstream ss;
		ss << "Fail to read page " << unipage.page << " in file: " << finfo.abs_path;
		if (ferror(finfo.fd)) {
			ss << ". error code(" << errno << ").";
		}
		else {
			ss << ". Unexpected end of file.";
		}
		
		clearerr(finfo.fd);
		throw std::ios_base::failure(ss.str());
	}*/

	pages_[unipage] = page;
	return page;
}

void BufferManager::FlushPageToDisk(Page* page, PageId pid) {
	auto piggy = static_cast<PagePiggyback*>(page->piggyback);
	auto& ios = GetStream(piggy->finfo);
	ios.SeekPut(pid * Page::kPageSize, std::ios::beg);
	ios.Write(page, Page::kPageSize);
	ios.Flush();
	page->is_dirty = false;
}

void BufferManager::FlushFileHeaderToDisk(FileId fid)
{
	auto& finfo = file_infos_[fid];
	auto& ios = GetStream(finfo);
	ios.SeekPut(0, std::ios::beg);
	ios.Write(&finfo.header, sizeof(finfo.header));
	ios.Flush();
}

void BufferManager::UnloadPage(UniquePage unipage)
{
	auto itr = pages_.find(unipage);
	if (itr == pages_.end()) {
		return;
	}

	if (itr->second->is_dirty) {
		FlushPageToDisk(itr->second, unipage.page);
	}

	DeletePage(itr->second);

	pages_.erase(itr);
}

BufferManager::~BufferManager()
{
	for (auto itr = pages_.begin(); itr != pages_.end(); ) {
		UnloadPage((itr++)->first);
	}

	/*
	for (auto finfo : file_infos_) {
		finfo.fd = nullptr;
	}*/
	
	// STLs are expcted to do the rest jobs
}

PageId BufferManager::AllocatePageAfter(FileId fid, PageId prev)
{
	auto populate_adjust_page = [this, &fid, &prev](PageId current) -> Page* {
		Page* page = nullptr;
		// we are poupulate a fresh new page
		if (prev.id == 0) {
			page = GetEmptyPage(0, 0);
		}
		else {
			// get previous page
			Page* prev_page = AutoFetchPage(UniquePage{
				prev,
				fid
				});

			// update previous page's valude pointing to self
			int next = prev_page->header.next;
			prev_page->header.next = current - prev;
			prev_page->is_dirty = true;

			// get next page
			int next_offset = 0;
			if (next != 0) {
				Page* next_page = AutoFetchPage(UniquePage{
					PageId(prev + next),
					fid
					});
				next_page->header.prev = current - next;
				next_page->is_dirty = true;

				next_offset = next - current;
			}
			page = GetEmptyPage(prev - current, next_offset);
		}
		page->header.clear_flag(Page::kfIsUnused);
		return page;
	};

	auto& finfo = file_infos_[fid];
	if (finfo.header.first_free.id == 0) {
		++finfo.header.num_pages;
		if (finfo.header.num_pages > PageId::kMax) {
			throw std::overflow_error("Too many pages in file");
		}
		PageId current = static_cast<PageId::IdType>(finfo.header.num_pages);


		long end_of_file = current * Page::kPageSize;
		auto& ios = GetStream(finfo);
		
		
		auto page = populate_adjust_page(current);
		ios.SeekPut(end_of_file, std::ios::beg);
		ios.Write(page, Page::kPageSize);
		
		// auto after_write_size = ios.TellPut();
		// if (after_write_size != (current.id + 1) * Page::kPageSize) {
		// 	throw std::ios_base::failure("Something went wrong when writing to file");
		// }
		
		return current;
	}

	PageId result = finfo.header.first_free;
	Page* page = AutoFetchPage(UniquePage{
		result,
		fid
		});

	auto& header = page->header;
	if (!header.flag(Page::kfIsUnused)) {
		std::stringstream ss;
		ss << "File: " << finfo.abs_path
			<< " at page " << finfo.header.first_free
			<< " is expected to be unused, but page says it is used, please check for file corruption";
		throw std::ios_base::failure(ss.str());
	}

	Page* adjusted_page = populate_adjust_page(result);

	memcpy(&page->header, &adjusted_page->header, sizeof(header));

	if (header.next == 0) {
		finfo.header.first_free = 0;
	}
	else {
		finfo.header.first_free.id += header.next;
	}
	
	FlushPageToDisk(page, result);
	FlushFileHeaderToDisk(fid);
	return result;
}

Page* BufferManager::GetEmptyPage(int prev, int next)
{
	if (empty_page_ == nullptr) {
		empty_page_ = new Page;
		empty_page_->is_dirty = false;
		empty_page_->header.set_flag(Page::kfIsUnused);
	}
	empty_page_->header.prev = prev;
	empty_page_->header.next = next;
	return empty_page_;
}

Page::Header BufferManager::GetPageHeader(FileId fid, PageId pid)
{
	UniquePage unipage{ pid, fid };
	auto itr = pages_.find(unipage);
	if (itr != pages_.end()) {
		return itr->second->header;
	}

	Page::Header header;
	auto& ios = GetStream(file_infos_[fid]);
	ios.SeekGet(pid * Page::kPageSize, std::ios::beg);
	ios.Read(&header, sizeof(header));
	return header;
}

void BufferManager::DeletePage(Page* page)
{
	auto piggy = static_cast<PagePiggyback*>(page->piggyback);
	delete piggy;
	delete page;
}





>>>>>>> 303d85540a4e8be07af52cf63b4766fef35e9063
