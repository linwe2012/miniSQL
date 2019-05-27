#pragma once
#include <set>
#include <map>
#include <string>
#include <vector>
#include <memory>

#include "sqldata.h"
#include "buffer-manager.h"



struct ItemPayload {
	BufferManager::Iterator<char> target;
	const char* data;
	int size_by_bytes;
	bool is_variadic;
};

// current just ignore that
struct Authorization {
	struct Allowed {
		int authid;
		std::string role_name;
	};

	std::vector<Allowed> allowed;
};

// descriptor for each column
struct Attribute {
	int id;
	int type;
	std::string column_name;

	std::string comment;

	Authorization authorization;

	FileId file;
	PageId first_page;
	PageId first_index;

	bool nullable;
	bool is_primary_key;
};

// a descriptor of table
struct MetaData {
	std::string db_name;
	std::string table_name;

	FileId file;
	std::map<std::string, Attribute> attributes;
	std::vector<int> primary_keys;

	Authorization authorization;
};




////////////////////////////////////
/////    Results of Query    ///////
////////////////////////////////////

struct ReportMsg {
	std::string detail;
	int code;
};

struct Result {
	std::vector<ReportMsg> report_msg;

	bool ok;
	int64_t num_rows_affected;

	const std::shared_ptr<MetaData> meta;
};


using ResultPtr = const std::shared_ptr<Result>;

struct AliasedTableName {
	std::string name;
	std::string alias;
	bool operator==(const AliasedTableName& rhs) const {
		return name == rhs.name || name == rhs.alias ||
			alias == rhs.name || alias == rhs.alias;
	}
};



class ConditionClause {
	
};
struct BinOpClause {
	enum Type {
		kOr,
		kAnd,
	};

	ConditionClause* lhs;
	ConditionClause* rhs;
};


struct RangeClause : public ConditionClause {
	Attribute* target;
	ISQLData* lower_bound;
	ISQLData* upper_bound;
	bool is_not;
	bool Test(ISQLData* data) {
		auto le = lower_bound->Compare(data);
		if (le == ISQLData::kLess || le == ISQLData::kEqual) {
			auto ge = lower_bound->Compare(data);
			if (ge == ISQLData::kLarger || ge == ISQLData::kEqual) {
				return true;
			}
		}
		return false;
	}
};

struct SetClause : public ConditionClause {
	std::set<ISQLData*> set;
};

class WhereClause {
	struct And {
		std::vector<ConditionClause> ands;
	};
	std::vector<And> ors;


};

struct Condition {

};



// ResultPtr Select(Table* table, const Condition& condition);
// ResultPtr Insert(Table* table, const RowData& row);
// ResultPtr Update(Table* table, const RowData& row, const Condition& condition);
// ResultPtr Delete(Table* table, const Condition& condition);
// ResultPtr BuildTable(const char* table_name);
// ResultPtr DropTable(Table* table);
// ResultPtr BuildIndex(Table* table, std::string column_name);
// ResultPtr DropIndex(Table* table, std::string column_name);


// calls lexer and call appropriate functions incl. insert/ delete etc.
ResultPtr Query(const char* query);

class DataPage {
	enum Type : uint8_t {
		kIndexed, // node of b+ tree
		kInode,   // index page, i.e. b+ tree
	};
	enum Format : uint8_t {
		kHamburger
	};
	enum Compress : uint8_t {
		kNotCompressed
	};
	int checksum;
	uint32_t offset;
	uint32_t prev;
	uint32_t next;
	uint64_t log_sqn;
	uint32_t space_id;
	Compress compress;
	Format format;
	Type type;
	

	uint16_t last_insert;
	

};

class MetaPage {
	
};




// using FileId = GenericIOId<uint32_t, 0>;













