#include "pch.h"
#include "../Database0/buffer-manager.h"

const char* db_file_name = "./test.db"; //db_ file for test

TEST(TestCaseName, TestName) {
  EXPECT_EQ(1, 1);
  EXPECT_TRUE(true);
}


TEST(BufferManger, IOFixed_NumberIsGood) {
	srand(0);
	FileId db_file;
	BufferManager bm;
	constexpr int fill_up = Page::kDiscretionSpace / sizeof(double) + 100;
	
	// write data
	if (!fs::exists(db_file_name)) {
		db_file = bm.NewFile(db_file_name);
		// allocate first page for column A
		PageId column_A_1 = bm.AllocatePageAfter(db_file, 0);
		PageId column_Var = bm.AllocatePageAfter(db_file, 0);
		PageId column_A_2 = bm.AllocatePageAfter(db_file, column_A_1);
		// get an iterator on the page
		auto itr = bm.GetPage<double>(db_file, column_A_1);

		// char* 是一个特殊的迭代器，表示变长的数据，边长数据理论上可以存放任何类型的数据，但会占更多空间
		// auto v = bm.GetPage<char*>(db_file, column_Var);
		auto last_page = &itr.page();
		for (int i = 0; i < fill_up; ++i) {
			itr.Insert((double)rand() / RAND_MAX);
			if (&itr.page() != last_page) {
				std::cerr << "We've just walk into another page!!" << std::endl;
				last_page = &itr.page();
			}
			++itr;
		}
	}
	else {
		db_file = bm.OpenFile(db_file_name);

		auto itr = bm.GetPage<double>(db_file, 1);
		//auto v = bm.GetPage<char*>(db_file, 2);
		for (int i = 0; i < fill_up; ++i) {
			EXPECT_EQ(*itr, (double)rand() / RAND_MAX);
			// std::cout << "row " << i << ": " << *itr << std::endl;
			++itr;
		}

		// for (int i = 0; i < 10; ++i) {
		// 	std::cout << "row " << i << ": " << v.AsString() << std::endl;
		// 	++v;
		// }
	}

}

TEST(BufferManger, IODeletePage) {
	BufferManager bm;
	const char* file = "io_deletepage.db";
	FileId db;
	if (!fs::exists(file)) {
		db = bm.NewFile(file);

		auto page1 = bm.AllocatePageAfter(db, 0);
		auto page2 = bm.AllocatePageAfter(db, page1);
		auto page3 = bm.AllocatePageAfter(db, page2);

		auto itr = bm.GetPage<double>(db, page1);

		while(itr.IsEnd())
	}

}

int main(int argc, char** argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}