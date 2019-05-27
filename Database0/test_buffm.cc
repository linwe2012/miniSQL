#include "buffer-manager.h"
#include "catalog-manager.h"
#include <iostream>

int main()
{
	BufferManager bm;
	const char* db_file_name = "./test.db";
	FileId db_file;
	std::vector<std::string> test;
	test.push_back("This is test A .....");
	test.push_back("db_file = bm.NewFile(db_file_name);");
	test.push_back("auto v = bm.GetPage<char*>(db_file, column_Var);");
	test.push_back("for (int i = 0; i < 10; ++i) {");
	test.push_back("++itr;");
	test.push_back("itr.Insert(10.0 * i);");
	srand(0);


	CatalogManager catalog;
	constexpr int fill_up = Page::kDiscretionSpace / sizeof(double) + 100;
	// write data
	if (!fs::exists(db_file_name)) {
		db_file = bm.NewFile(db_file_name);
		PageId catalog_page = bm.AllocatePageAfter(db_file, 0);
		

		// allocate first page for column A
		PageId column_A_1 = bm.AllocatePageAfter(db_file, 0);
		PageId column_Var = bm.AllocatePageAfter(db_file, 0);
		PageId column_A_2 = bm.AllocatePageAfter(db_file, column_A_1);



		auto cat_itr = bm.GetPage<char*>(db_file, catalog_page);
		Attribute dummy{ 0, 1, std::string(), std::string(), Authorization(), FileId(), PageId(), PageId() };
		Attribute attrib_id = dummy;
		dummy.id = 1; dummy.type = 2;
		Attribute attrib_name = dummy;

		MetaData meta{
			std::string("library"),
			std::string("user"),
			db_file,
			std::map<std::string, Attribute> {
				{std::string("id"), attrib_id},
				{std::string("name"), attrib_name}
			},

			std::vector{
				0 //0 is primary key
			},

			Authorization()
		};


		catalog.SerializeOneTable(cat_itr, meta);

		// rewind to beginning of page
		cat_itr = bm.GetPage<char*>(db_file, catalog_page);

		MetaData fetched_meta;
		catalog.DeserializeMeta(cat_itr, fetched_meta);
		MetaData& tb = fetched_meta;

		std::cout << "Database: " << tb.db_name
			<< "\nTable: " << tb.table_name << std::endl;

		for (auto& it : tb.attributes) {
			auto& attrib = it.second;
			std::cout << it.first << " " << attrib.id << " " << attrib.type
				<< " " << attrib.column_name;
		}


		// get an iterator on the page
		auto itr = bm.GetPage<double>(db_file, column_A_1);

		// char* 是一个特殊的迭代器，表示变长的数据，边长数据理论上可以存放任何类型的数据，但会占更多空间
		// auto v = bm.GetPage<char*>(db_file, column_Var);
		auto last_page = &itr.page();
		for (int i = 0; i < fill_up; ++i) {
			if (!itr.Insert(/* (double)rand() / RAND_MAX*/ double(i))) {
				++itr;
				itr.Insert(double(i));
			}
			
			if (&itr.page() != last_page) {
				std::cerr << "We've just walk into another page!!" << std::endl;
				last_page = &itr.page();
				getchar();
			}
			++itr;
		}

		


		//for (int i = 0; i < 10; ++i) {
		//	v.Insert(test[i % test.size()]);
		//	++v;
		//}
	}
	// read data
	else {
		db_file = bm.OpenFile(db_file_name);

		// rewind to beginning of page
		auto cat_itr = bm.GetPage<char*>(db_file, 1);

		MetaData fetched_meta;
		catalog.DeserializeMeta(cat_itr, fetched_meta);
		MetaData& tb = fetched_meta;

		std::cout << "Database: " << tb.db_name
			<< "\nTable: " << tb.table_name << std::endl;

		for (auto& it : tb.attributes) {
			auto& attrib = it.second;
			std::cout << it.first << ":\t" << attrib.id << "\t" << attrib.type
				<< "\t" << attrib.column_name << std::endl;
		}

		


		// 因为之前分配的column 的PageId 是 1,这里偷懒直接用1, 这应该由 Catalog Manager 记录
		// Catalog Manager 知道表中每一列对应的第一个PageId, 查询列的时候应该返回一个指向第一行的迭代器
		// Index Manager 应该记录哪些Key对应哪一个PageId, 查询Key的时候返回 一个迭代器
		
		//TODO 计划在 单个Page 上实现二分查找
		auto itr = bm.GetPage<double>(db_file, 2);
		auto v = bm.GetPage<char*>(db_file, 3);
		auto itr2 = bm.GetPage<double>(db_file, 4);
		for (int i = 0; i < fill_up; ++i) {
			
			if (*itr != double(i) /*(double)rand() / RAND_MAX*/) {
				std::cout << "row " << i << " fail" << std::endl;
			}
			++itr;
			if (itr.IsEndPage()) {
				++itr;
			}
		}
		
		getchar();
		getchar();
	}

	

}

