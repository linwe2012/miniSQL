#include "buffer-manager.h"
#include "catalog-manager.h"
#include <iostream>

#include "execute.h"
#include "pretty-print.h"
#include <fstream>
#include <Windows.h>

PageId TestWriteIndex();
void TestReadIndex(PageId);
void TestCatalogManager();
void TestCatalogRead();


int main()
{
	

	{
		BufferManager bm;
		CatalogManager cat;
		FunctionsManager fm;
		FileId file;
		if (fs::exists("./test_user.db")) {
			file = bm.OpenFile("./test_user.db");
			//fs::remove("./test_user.db");
		}
		else {
			file = bm.NewFile("./test_user.db");
			bm.AllocatePageAfter(file, 0);
		}
		cat.Initialize(&bm, file, 1);
		fm.Initilize();
		ExecuteContext ec;
		// ec.db = "heaven";
		ec.bm = &bm;
		ec.cat = &cat;
		ec.fun = &fm;
		std::ostream* os = &std::cout;
		std::istream* is = &std::cin;
		std::ifstream ifs;
		std::vector<int> state_flag;
		while (1)
		{
			(*os) << "$ ";
			std::stringstream ss;
			std::string str;
			std::getline(*is, str);
			
			
			if (str == "show tables") {
				PrintTables(cat, ec.db, os);
				continue;
			}
			if (str == "quit") {
				return 0;
			}

			if (str.size() && str[0] == '@') {
				
				auto vec = Explode(str, ' ');
				if (vec[0] == "@echo") {
					(*os) << str.substr(6);
					continue;
				}
			}

			Parser parser;
			try {
				parser.ParseExpression(str.c_str());
			}
			catch (SyntaxError& s) {
				(*os) << "[Syntax]: " << str << std::endl;
				(*os) << "          ";
				int i = 0;
				for (; i < s.last_char_cnt; ++i) {
					(*os) << " ";
				}
				for (; i < s.char_cnt; ++i) {
					(*os) << "~";
				}
				
				(*os) << std::endl;
				continue;
			}
			
			if (parser.target.type == Token::kExecFile) {
				std::string file = str.substr(str.find_first_of(' ') + 1);
				if (file[file.size()-1] == ';') {
					file = file.substr(0, file.size() - 1);
				}
				ifs.open(file);
				if (!ifs.is_open()) {
					std::cout << "unable to open file" << std::endl;
					continue;
				}
				is = &ifs;
				
				continue;
			}
			else if (ifs.eof() || !ifs.good()) {
				bool test = ifs.eof();
				is = &std::cin;
				ifs.close();
			}

			auto res = Execute(ec, parser.target);
			
			if (res->ok) {
				SQLDataPrettyPrinter printer;
				printer.Bind(&std::cout);
				
				for (auto a : res->attribs) {
					std::cout << a->column_name << '\t';
				}
				if (res->attribs.size()) {
					std::cout << '\n';
					std::cout << res->requested.size() << " rows fetched "
						<< std::endl;
				}

				for (auto row : res->requested) {
					for (auto item : row) {
						item->Accept(&printer);

					}
					std::cout << '\n';
				}
				std::cout << "--" << res->span.count() << " miliseconds used--" << std::endl;
			}
			
		}
	}
	
	{
		// PageId p = TestWriteIndex();
		// TestReadIndex(822);
	}
	
	{
		TestCatalogManager();
		TestCatalogRead();
	}

	{
		BufferManager bm;
		CatalogManager cat;
		FunctionsManager fm;
		FileId file = bm.OpenFile("./test_cat.db");
		cat.Initialize(&bm, file, 1);
		fm.Initilize();

		ExecuteContext ec;
		ec.db = "heaven";
		ec.bm = &bm;
		ec.cat = &cat;
		ec.fun = &fm;


		{
			Parser parser;
			parser.ParseExpression("INSERT into divine (id, name, witch_cratf) values (0, 'Adonael', 20)");
			Execute(ec, parser.target);
		}
		{
			Parser parser;
			parser.ParseExpression("INSERT into divine (id, name, witch_cratf) values (1, 'Michael', 20)");
			Execute(ec, parser.target);
		}
		{
			Parser parser;
			parser.ParseExpression("INSERT into divine (id, name, witch_cratf, age) values (2, 'Gabriel', 20, 1090)");
			Execute(ec, parser.target);
		}

		{
			Parser parser;
			parser.ParseExpression("INSERT into enroll (id, grade) values (1, 20)");
			Execute(ec, parser.target);
		}

		{
			Parser parser;
			parser.ParseExpression("INSERT into enroll (id, grade) values (0, 30)");
			Execute(ec, parser.target);
		}
		
		{
			Parser parser;
			parser.ParseExpression("INSERT into enroll (id, grade) values (2, 40)");
			Execute(ec, parser.target);
		}

		{
			Parser parser;
			parser.ParseExpression("select name, age from divine, enroll where enroll.id = divine.id and grade > 20");
			auto res = Execute(ec, parser.target);
			SQLDataPrettyPrinter printer;
			printer.Bind(&std::cout);

			for (auto a : res->attribs) {
				std::cout << a->column_name << '\t';
			}
			std::cout << '\n';

			for (auto row : res->requested) {
				for (auto item : row) {
					item->Accept(&printer);
					
				}
				std::cout << '\n';
			}
			int cc = 0;
		}
	}

}

#include "cluster-bptree.h"

PageId TestWriteIndex() {
	BufferManager bm;
	if (fs::exists("./test2.db")) {
		fs::remove("./test2.db");
	}
	FileId file = bm.NewFile("./test2.db");

	PageId page = bm.AllocatePageAfter(file, 0);

	ClusteredBPTree<double> dt(&bm, file, 0);
	auto root_index = dt.BuildIndex(bm.GetPage<double>(file, page));
	for (double i = 0.1; i < 10000; i += 0.01) {
		if (i >= 3173.82) {
			int cc = 10;
		}
		root_index = dt.Insert(i);
	}

	return root_index;
}

void TestA() {
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


	constexpr int fill_up = Page::kDiscretionSpace / sizeof(double) + 100;
	// write data
	if (!fs::exists(db_file_name)) {
		db_file = bm.NewFile(db_file_name);
		PageId catalog_page = bm.AllocatePageAfter(db_file, 0);


		// allocate first page for column A
		PageId column_A_1 = bm.AllocatePageAfter(db_file, 0); (void)column_A_1;
		PageId column_Var = bm.AllocatePageAfter(db_file, 0); (void)column_Var;
		PageId column_A_2 = bm.AllocatePageAfter(db_file, column_A_1); (void)column_A_2;



		auto cat_itr = bm.GetPage<char*>(db_file, catalog_page);
		Attribute dummy{ 0, 1, std::string(), std::string(), std::string(), Authorization(), FileId(), PageId(), PageId() };
		Attribute attrib_id = dummy;
		dummy.id = 1; dummy.type = 2;
		Attribute attrib_name = dummy;

		/*
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
		*/

		// catalog.SerializeOneTable(cat_itr, meta);
		// 
		// // rewind to beginning of page
		// cat_itr = bm.GetPage<char*>(db_file, catalog_page);
		// 
		// MetaData fetched_meta;
		// catalog.DeserializeMeta(cat_itr, fetched_meta);
		// MetaData& tb = fetched_meta;
		// 
		// std::cout << "Database: " << tb.db_name
		// 	<< "\nTable: " << tb.table_name << std::endl;
		// 
		// for (auto& it : tb.attributes) {
		// 	auto& attrib = it.second;
		// 	std::cout << it.first << " " << attrib.id << " " << attrib.type
		// 		<< " " << attrib.column_name;
		// }


		// get an iterator on the page
		auto itr = bm.GetPage<double>(db_file, column_A_1);

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
	}

	bm.~BufferManager();
	
}
void TestReadIndex(PageId root) {
	BufferManager bm;
	
	FileId file = bm.OpenFile("./test2.db");

	ClusteredBPTree<double> dt(&bm, file, root);

	for (double i = 0.1; i < 10000; i += 0.01) {
		if (i >= 3713.82) {
			int cc = 10;
		}
		auto itr = dt.Lookup(i);
		assert(*itr == i);
	}
}


void TestCatalogManager() {
	BufferManager bm;
	CatalogManager cat;

	if (fs::exists("./test_cat.db")) {
		fs::remove("./test_cat.db");
	}

	FileId file = bm.NewFile("./test_cat.db");
	PageId page = bm.AllocatePageAfter(file, 0);

	std::string db_name = "heaven";
	std::string tb_name = "mortal";

	cat.Initialize(&bm, file, page);

	ColumnInfo col;
	col.comment = "hahaha";
	col.name.db_name = db_name;
	col.name.table_name = tb_name;
	col.name.column_name = "id";
	col.type = SQLTypeID<SQLBigInt>::value;
	col.is_primary_key = true;
	col.max_length = -1;
	col.nullable = false;


	std::vector<ColumnInfo> cols;
	cols.push_back(col);

	col.name.column_name = "name";
	col.is_primary_key = false;
	col.type = SQLTypeID<SQLString>::value;
	cols.push_back(col);

	col.type = SQLTypeID<SQLBigInt>::value;
	col.name.column_name = "age";
	col.nullable = true;
	cols.push_back(col);

	col.name.column_name = "witch_cratf";
	cols.push_back(col);

	cat.NewTable(db_name, tb_name, cols);

	tb_name = "spirit";
	col.name.column_name = "siprit_witch_cratf";
	cols.push_back(col);
	cat.NewTable(db_name, tb_name, cols);

	tb_name = "divine";
	cols.pop_back();
	cat.NewTable(db_name, tb_name, cols);

	tb_name = "enroll";
	cols.clear();
	col.name.column_name = "id";
	col.is_primary_key = true;
	cols.push_back(col);

	col.name.column_name = "grade";
	col.is_primary_key = false;
	cols.push_back(col);
	cat.NewTable(db_name, tb_name, cols);
}

void PrtColumn(const Attribute& a) {
	std::cout << a.column_name << "\t" << a.comment << "\t primary: "
		<< a.is_primary_key << "\t nullable: " << a.nullable << "\t max: " << a.max_length;
}

void PrtTable(const MetaData& meta) {
	std::cout << "db: " << meta.db_name
		<< "table: " << meta.table_name << std::endl;

	for (const auto& attrib : meta.attributes) {
		PrtColumn(attrib);
		std::cout << std::endl;
	}
}

void TestCatalogRead() {
	BufferManager bm;
	CatalogManager cat;
	FileId file = bm.OpenFile("./test_cat.db");
	cat.Initialize(&bm, file, 1);

	// welcome to college of heaven
	PrtTable(cat.FetchTable("heaven", "mortal"));
	PrtTable(cat.FetchTable("heaven", "spirit"));
	PrtTable(cat.FetchTable("heaven", "divine"));
	PrtTable(cat.FetchTable("heaven", "enroll"));

	int break_point = 0;
}




// INSERT into divine(id, name, witch_cratf) values     (20, ' 2Adonael', 20)
// INSERT into divine(id, name, witch_cratf) values     (21, ' 2Michael', 20)
// INSERT into divine(id, name, witch_cratf, age) values(22, ' 2Gabriel', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (23, ' 2Adam', 20)
// INSERT into divine(id, name, witch_cratf) values     (24, ' 2Qubiz', 20)
// INSERT into divine(id, name, witch_cratf, age) values(25, ' 2Galla', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (26, ' 2Fesel', 20)
// INSERT into divine(id, name, witch_cratf) values     (27, ' 2Achtung', 20)
// INSERT into divine(id, name, witch_cratf, age) values(28, ' 2Man', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (29, ' 2Shar', 20)
// INSERT into divine(id, name, witch_cratf) values     (30, ' 3Adonael', 20)
// INSERT into divine(id, name, witch_cratf) values     (31, ' 3Michael', 20)
// INSERT into divine(id, name, witch_cratf, age) values(32, ' 3Gabriel', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (33, ' 3Adam', 20)
// INSERT into divine(id, name, witch_cratf) values     (34, ' 3Qubiz', 20)
// INSERT into divine(id, name, witch_cratf, age) values(35, ' 3Galla', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (36, ' 3Fesel', 20)
// INSERT into divine(id, name, witch_cratf) values     (37, ' 3Achtung', 20)
// INSERT into divine(id, name, witch_cratf, age) values(38, ' 3Man', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (39, ' 3Shar', 20)
// INSERT into divine(id, name, witch_cratf) values     (40, ' 4Adonael', 20)
// INSERT into divine(id, name, witch_cratf) values     (41, ' 4Michael', 20)
// INSERT into divine(id, name, witch_cratf, age) values(42, ' 4Gabriel', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (43, ' 4Adam', 20)
// INSERT into divine(id, name, witch_cratf) values     (44, ' 4Qubiz', 20)
// INSERT into divine(id, name, witch_cratf, age) values(45, ' 4Galla', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (46, ' 4Fesel', 20)
// INSERT into divine(id, name, witch_cratf) values     (47, ' 4Achtung', 20)
// INSERT into divine(id, name, witch_cratf, age) values(48, ' 4Man', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (49, ' 4Shar', 20)
// INSERT into divine(id, name, witch_cratf) values     (50, ' 5Adonael', 20)
// INSERT into divine(id, name, witch_cratf) values     (51, ' 5Michael', 20)
// INSERT into divine(id, name, witch_cratf, age) values(52, ' 5Gabriel', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (53, ' 5Adam', 20)
// INSERT into divine(id, name, witch_cratf) values     (54, ' 5Qubiz', 20)
// INSERT into divine(id, name, witch_cratf, age) values(55, ' 5Galla', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (56, ' 5Fesel', 20)
// INSERT into divine(id, name, witch_cratf) values     (57, ' 5Achtung', 20)
// INSERT into divine(id, name, witch_cratf, age) values(58, ' 5Man', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (59, ' 5Shar', 20)
// INSERT into divine(id, name, witch_cratf) values     (60, '  6Adonael', 20)
// INSERT into divine(id, name, witch_cratf) values     (61, '  6Michael', 20)
// INSERT into divine(id, name, witch_cratf, age) values(62, '  6Gabriel', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (63, '  6Adam', 20)
// INSERT into divine(id, name, witch_cratf) values     (64, '  6Qubiz', 20)
// INSERT into divine(id, name, witch_cratf, age) values(65, '  6Galla', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (66, '  6Fesel', 20)
// INSERT into divine(id, name, witch_cratf) values     (67, '  6Achtung', 20)
// INSERT into divine(id, name, witch_cratf, age) values(68, '  6Man', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (69, '  6Shar', 20)
// INSERT into divine(id, name, witch_cratf) values     (70, '  7 Adonael', 20)
// INSERT into divine(id, name, witch_cratf) values     (71, '  7 Michael', 20)
// INSERT into divine(id, name, witch_cratf, age) values(72, '  7 Gabriel', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (73, '  7 Adam', 20)
// INSERT into divine(id, name, witch_cratf) values     (74, '  7 Qubiz', 20)
// INSERT into divine(id, name, witch_cratf, age) values(75, '  7 Galla', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (76, '  7 Fesel', 20)
// INSERT into divine(id, name, witch_cratf) values     (77, '  7 Achtung', 20)
// INSERT into divine(id, name, witch_cratf, age) values(78, '  7 Man', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (79, '  7 Shar', 20)
// INSERT into divine(id, name, witch_cratf) values     (80, '  8 Adonael', 20)
// INSERT into divine(id, name, witch_cratf) values     (81, '  8 Michael', 20)
// INSERT into divine(id, name, witch_cratf, age) values(82, '  8 Gabriel', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (83, '  8 Adam', 20)
// INSERT into divine(id, name, witch_cratf) values     (84, '  8 Qubiz', 20)
// INSERT into divine(id, name, witch_cratf, age) values(85, '  8 Galla', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (86, '  8 Fesel', 20)
// INSERT into divine(id, name, witch_cratf) values     (87, '  8 Achtung', 20)
// INSERT into divine(id, name, witch_cratf, age) values(88, '  8 Man', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (89, '  8 Shar', 20)
// INSERT into divine(id, name, witch_cratf) values     (90, '  9 Adonael', 20)
// INSERT into divine(id, name, witch_cratf) values     (91, '  9 Michael', 20)
// INSERT into divine(id, name, witch_cratf, age) values(92, '  9 Gabriel', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (93, '  9 Adam', 20)
// INSERT into divine(id, name, witch_cratf) values     (94, '  9 Qubiz', 20)
// INSERT into divine(id, name, witch_cratf, age) values(95, '  9 Galla', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (96, '  9 Fesel', 20)
// INSERT into divine(id, name, witch_cratf) values     (97, '  9 Achtung', 20)
// INSERT into divine(id, name, witch_cratf, age) values(98, '  9 Man', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (99, '  9 Shar', 20)

// INSERT into divine(id, name, witch_cratf) values     (120, '  12Adonael', 20)
// INSERT into divine(id, name, witch_cratf) values     (121, '  12Michael', 20)
// INSERT into divine(id, name, witch_cratf, age) values(122, '  12Gabriel', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (123, '  12Adam', 20)
// INSERT into divine(id, name, witch_cratf) values     (124, '  12Qubiz', 20)
// INSERT into divine(id, name, witch_cratf, age) values(125, '  12Galla', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (126, '  12Fesel', 20)
// INSERT into divine(id, name, witch_cratf) values     (127, '  12Achtung', 20)
// INSERT into divine(id, name, witch_cratf, age) values(128, '  12Man', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (129, '  12Shar', 20)
// INSERT into divine(id, name, witch_cratf) values     (130, '  13Adonael', 20)
// INSERT into divine(id, name, witch_cratf) values     (131, '  13Michael', 20)
// INSERT into divine(id, name, witch_cratf, age) values(132, '  13Gabriel', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (133, '  13Adam', 20)
// INSERT into divine(id, name, witch_cratf) values     (134, '  13Qubiz', 20)
// INSERT into divine(id, name, witch_cratf, age) values(135, '  13Galla', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (136, '  13Fesel', 20)
// INSERT into divine(id, name, witch_cratf) values     (137, '  13Achtung', 20)
// INSERT into divine(id, name, witch_cratf, age) values(138, '  13Man', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (139, '  13Shar', 20)
// INSERT into divine(id, name, witch_cratf) values     (140, '  14Adonael', 20)
// INSERT into divine(id, name, witch_cratf) values     (141, '  14Michael', 20)
// INSERT into divine(id, name, witch_cratf, age) values(142, '  14Gabriel', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (143, '  14Adam', 20)
// INSERT into divine(id, name, witch_cratf) values     (144, '  14Qubiz', 20)
// INSERT into divine(id, name, witch_cratf, age) values(145, '  14Galla', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (146, '  14Fesel', 20)
// INSERT into divine(id, name, witch_cratf) values     (147, '  14Achtung', 20)
// INSERT into divine(id, name, witch_cratf, age) values(148, '  14Man', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (149, '  14Shar', 20)
// INSERT into divine(id, name, witch_cratf) values     (150, '  15Adonael', 20)
// INSERT into divine(id, name, witch_cratf) values     (151, '  15Michael', 20)
// INSERT into divine(id, name, witch_cratf, age) values(152, '  15Gabriel', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (153, '  15Adam', 20)
// INSERT into divine(id, name, witch_cratf) values     (154, '  15Qubiz', 20)
// INSERT into divine(id, name, witch_cratf, age) values(155, '  15Galla', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (156, '  15Fesel', 20)
// INSERT into divine(id, name, witch_cratf) values     (157, '  15Achtung', 20)
// INSERT into divine(id, name, witch_cratf, age) values(158, '  15Man', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (159, '  15Shar', 20)
// INSERT into divine(id, name, witch_cratf) values     (160, '  16Adonael', 20)
// INSERT into divine(id, name, witch_cratf) values     (161, '  16Michael', 20)
// INSERT into divine(id, name, witch_cratf, age) values(162, '  16Gabriel', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (163, '  16Adam', 20)
// INSERT into divine(id, name, witch_cratf) values     (164, '  16Qubiz', 20)
// INSERT into divine(id, name, witch_cratf, age) values(165, '  16Galla', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (166, '  16Fesel', 20)
// INSERT into divine(id, name, witch_cratf) values     (167, '  16Achtung', 20)
// INSERT into divine(id, name, witch_cratf, age) values(168, '  16Man', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (169, '  16Shar', 20)
// INSERT into divine(id, name, witch_cratf) values     (170, '  17 Adonael', 20)
// INSERT into divine(id, name, witch_cratf) values     (171, '  17 Michael', 20)
// INSERT into divine(id, name, witch_cratf, age) values(172, '  17 Gabriel', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (173, '  17 Adam', 20)
// INSERT into divine(id, name, witch_cratf) values     (174, '  17 Qubiz', 20)
// INSERT into divine(id, name, witch_cratf, age) values(175, '  17 Galla', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (176, '  17 Fesel', 20)
// INSERT into divine(id, name, witch_cratf) values     (177, '  17 Achtung', 20)
// INSERT into divine(id, name, witch_cratf, age) values(178, '  17 Man', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (179, '  17 Shar', 20)
// INSERT into divine(id, name, witch_cratf) values     (180, '  18 Adonael', 20)
// INSERT into divine(id, name, witch_cratf) values     (181, '  18 Michael', 20)
// INSERT into divine(id, name, witch_cratf, age) values(182, '  18 Gabriel', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (183, '  18 Adam', 20)
// INSERT into divine(id, name, witch_cratf) values     (184, '  18 Qubiz', 20)
// INSERT into divine(id, name, witch_cratf, age) values(185, '  18 Galla', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (186, '  18 Fesel', 20)
// INSERT into divine(id, name, witch_cratf) values     (187, '  18 Achtung', 20)
// INSERT into divine(id, name, witch_cratf, age) values(188, '  18 Man', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (189, '  18 Shar', 20)
// INSERT into divine(id, name, witch_cratf) values     (190, '  19 Adonael', 20)
// INSERT into divine(id, name, witch_cratf) values     (191, '  19 Michael', 20)
// INSERT into divine(id, name, witch_cratf, age) values(192, '  19 Gabriel', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (193, '  19 Adam', 20)
// INSERT into divine(id, name, witch_cratf) values     (194, '  19 Qubiz', 20)
// INSERT into divine(id, name, witch_cratf, age) values(195, '  19 Galla', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (196, '  19 Fesel', 20)
// INSERT into divine(id, name, witch_cratf) values     (197, '  19 Achtung', 20)
// INSERT into divine(id, name, witch_cratf, age) values(198, '  19 Man', 20, 1090)
// INSERT into divine(id, name, witch_cratf) values     (199, '  19 Shar', 20)
