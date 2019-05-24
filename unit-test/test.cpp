#include "pch.h"
#include "../Database0/buffer-manager.h"

const char* db_file_name = "./test.db"; //db_ file for test

TEST(TestCaseName, TestName) {
  EXPECT_EQ(1, 1);
  EXPECT_TRUE(true);
}


TEST(IOFixed_NumberIsGood, BufferManger) {
	srand(0);
	FileId db_file;
	BufferManager bm;

	
	// write data
	if (!fs::exists(db_file_name)) {
		db_file = bm.NewFile(db_file_name);
		// allocate first page for column A
		PageId column_A = bm.AllocatePageAfter(db_file, 0);
		PageId column_Var = bm.AllocatePageAfter(db_file, 0);

		// get an iterator on the page
		auto itr = bm.GetPage<double>(db_file, column_A);

		// char* 是一个特殊的迭代器，表示变长的数据，边长数据理论上可以存放任何类型的数据，但会占更多空间
		// auto v = bm.GetPage<char*>(db_file, column_Var);
		for (int i = 0; i < 10; ++i) {
			itr.Insert((double)rand() / RAND_MAX);
			++itr;
		}
	}
	else {
		db_file = bm.OpenFile(db_file_name);

		auto itr = bm.GetPage<double>(db_file, 1);
		//auto v = bm.GetPage<char*>(db_file, 2);
		for (int i = 0; i < 10; ++i) {
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

int main(int argc, char** argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}