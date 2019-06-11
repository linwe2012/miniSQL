#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE
#endif // _MSC_VER

#include "buffer-manager.h"
#include <fstream>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#else
#ifndef O_BINARY
#define O_BINARY 0
#define DONT_HAVE_TELL
#endif
#include <unistd.h>
#endif



#ifdef  BUFFER_MANAGER_USE_STD_STREAM

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
};

#else // use unix style stream
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
		auto res = write(fd, data, static_cast<unsigned>(size));
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
		auto res = read(fd, buf, static_cast<unsigned>(size));
		if (res != size) {
			throw std::ios_base::failure("Read failed");
		}
	}

#pragma warning (push)
#pragma warning (disable: 4267)
	void SeekPut(size_t _pos, int way) {

		auto res = lseek(fd, _pos, SEEK_SET);
		if (way == std::ios::beg) {
			if (res != _pos) {
				throw std::ios_base::failure("Fail to seek");
			}
		}
	}

	void SeekGet(size_t _pos, int way) {
		auto res = lseek(fd, _pos, SEEK_SET);
		if (way == std::ios::beg) {
			if (res != _pos) {
				throw std::ios_base::failure("Fail to seek");
			}
		}
	}
#pragma warning (pop)

	void Flush() {

	}

	size_t TellGet() {
#ifdef DONT_HAVE_TELL
		return lseek(fd, 0, SEEK_CUR);
#else
		return tell(fd);
#endif
	}

	size_t TellPut() {
#ifdef DONT_HAVE_TELL
		return lseek(fd, 0, SEEK_CUR);
#else
		return tell(fd);
#endif
	}

	~FStream() {
		close(fd);
	}
private:
	int fd = -1;
	size_t pos = 0;
};
#endif //  BUFFER_MANAGER_USE_STD_STREAM





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
	}
	
	auto itr = loaded_files_.find(canon);
	if (itr != loaded_files_.end()) {
		return itr->second.id;
	}

	
	
	
	// TestFileOk(finfo.fd, finfo.abs_path);

	Page* page_id = GetEmptyPage(0, 0);

	auto& ios = GetStream(finfo);
	ios.NewFile(path);
	ios.Write(page_id, Page::kPageSize);

	finfo.abs_path = canon.string();
	// gcc 7.4 does not support weakly_canconial
	canon = fs::canonical(path);
	
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
	ppb->finfo = &finfo;
	ppb->page_id = unipage.page_id;

	Page* page_id = new Page;
	page_id->piggyback = ppb;

	auto& ios = GetStream(finfo);
	ios.SeekGet(unipage.page_id * (size_t)Page::kPageSize, std::ios::beg);
	ios.Read(page_id, Page::kPageSize);
	
	pages_[unipage] = page_id;
	return page_id;
}

void BufferManager::FlushPageToDisk(Page* page_id, PageId pid) {
	auto piggy = static_cast<PagePiggyback*>(page_id->piggyback);
	auto& ios = GetStream(*(piggy->finfo));
	ios.SeekPut(pid * Page::kPageSize, std::ios::beg);
	ios.Write(page_id, Page::kPageSize);
	ios.Flush();
	page_id->is_dirty = false;
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
		FlushPageToDisk(itr->second, unipage.page_id);
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
		FlushFileHeaderToDisk(finfo.id);
		delete &GetStream(finfo);
		finfo.fd = nullptr;
	}
	
	// STLs are expcted to do the rest jobs
}

PageId BufferManager::AllocatePageAfter(FileId fid, PageId prev, std::vector<int> flags)
{
	auto populate_adjust_page = [this, &fid, &prev, &flags](PageId current) -> Page* {
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
				next_page->header.prev = current - (prev + next);
				next_page->is_dirty = true;

				next_offset = (prev + next) - current;
			}
			page = GetEmptyPage(prev - current, next_offset);
		}
		page->header.clear_flag(Page::kfIsUnused);
		for (auto f : flags) {
			page->header.set_flag(f);
		}
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
		
		
		auto page_id = populate_adjust_page(current);
		ios.SeekPut(end_of_file, std::ios::beg);
		ios.Write(page_id, Page::kPageSize);
		
		return current;
	}

	PageId result = finfo.header.first_free;
	Page* page_id = AutoFetchPage(UniquePage{
		result,
		fid
		});

	auto& header = page_id->header;
	if (!header.flag(Page::kfIsUnused)) {
		std::stringstream ss;
		ss << "File: " << finfo.abs_path
			<< " at page " << finfo.header.first_free
			<< " is expected to be unused, but page says it is used, please check for file corruption";
		throw std::ios_base::failure(ss.str());
	}

	Page* adjusted_page = populate_adjust_page(result);

	memcpy(&page_id->header, &adjusted_page->header, sizeof(header));

	if (header.next == 0) {
		finfo.header.first_free = 0;
	}
	else {
		finfo.header.first_free.id += header.next;
	}
	
	FlushPageToDisk(page_id, result);
	FlushFileHeaderToDisk(fid);
	return result;
}

Page* BufferManager::GetEmptyPage(int prev, int next)
{
	if (empty_page_ == nullptr) {
		empty_page_ = new Page;
	}
	empty_page_->is_dirty = false;
	empty_page_->header.flags = 0;
	empty_page_->header.set_flag(Page::kfIsUnused);

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

void BufferManager::DeletePage(Page* page_id)
{
	auto piggy = static_cast<PagePiggyback*>(page_id->piggyback);
	delete piggy;
	delete page_id;
}


void BufferManager::IteratorDeletePage(Page* page_id) {
	auto piggy = static_cast<PagePiggyback*>(page_id->piggyback);
	auto& header = file_infos_[piggy->finfo->id].header;
	page_id->is_dirty = true;
	page_id->header.next = header.first_free - piggy->page_id;
	page_id->header.set_flag(Page::kfIsUnused);
	header.first_free = piggy->page_id;
	UnloadPage(UniquePage{ piggy->page_id, piggy->finfo->id });
}






