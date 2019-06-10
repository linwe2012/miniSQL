#pragma once
#include <set>
#include <map>
#include <string>
#include <vector>
#include <memory>

#include "sqldata.h"
#include "buffer-manager.h"
#include <chrono>

template <typename T>
class Maybe {
public:
	T* operator->() { return *target_; }
	T& operator*() { return **target_; }

	Maybe(T** location) {
		target_ = (location);
	}

	Maybe() : target_(nullptr) {}

	const T* operator->() const { return *target_; }
	const T& operator*() const  { return **target_; }

	bool IsNil() const { return target_ != nullptr && *target_ != nullptr; }

	bool operator==(const Maybe<T> rhs) const {
		if (IsNil()) {
			if (rhs.IsNil()) {
				return true;
			}
			else {
				return false;
			}
		}

		return operator->() == rhs.operator->();
	}

	T** location() {
		return target_;
	}


private:
	T** target_;
};

struct ItemPayload {
	BufferManager::Iterator<char> target;
	const char* data;
	uint64_t size_by_bytes;
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

	int max_length;

	bool nullable;
	bool is_primary_key;
};



// a descriptor of table
struct MetaData {
	std::string db_name;
	std::string table_name;

	FileId file;
	std::vector<Attribute> attributes;
	std::map<std::string, Attribute*> attributes_map;
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
	std::vector<const Attribute*> attribs;
	std::vector<std::vector<std::shared_ptr<ISQLData>>> requested;
	std::chrono::milliseconds span;
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

class MetaPage {
	
};




// using FileId = GenericIOId<uint32_t, 0>;













