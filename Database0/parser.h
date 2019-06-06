#pragma once

#include <string>
#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <sstream>

#include "token.h"

struct TableClause {
	std::string db_name;
	std::string name;
	std::string alias;
};

struct TableJoinClause {
	enum JoinMethod {
		kNatural,
		kOuter,
		kInnner,
	};
	TableClause table;
	JoinMethod method;
};


struct QueryClause {
	enum struct Type {
		Select,
		Insert,
		CreateTable,

	};

	std::string default_db_name;
	std::map<std::string, TableClause*> tables_;
	std::map < std::string, std::vector<TableJoinClause>> joins_;
	
	TableClause* GetTableByName(std::string db, std::string tb_or_alias) {
		if (tb_or_alias.size() == 0) {
			return nullptr;
		}
		if (db.size() == 0) {
			auto itr = tables_.find(default_db_name + "." + tb_or_alias);
			if (itr == tables_.end()) {
				return nullptr;
			}
			return itr->second;
		}

		auto itr = tables_.find(db + "." + tb_or_alias);
		if (itr == tables_.end()) {
			return nullptr;
		}
		return itr->second;
	}


	void AddTableClause(const TableClause& clause) {
		auto lmbdGetFullName = [this](const std::string& db, const std::string& tb) {
			if (db.size() == 0) {
				return default_db_name + "." + tb;
			}
			return db + "." + tb;
		};

		auto itr = tables_.find(lmbdGetFullName(clause.db_name, clause.name));
		auto aitr = tables_.find(lmbdGetFullName(clause.db_name, clause.alias));
		

		
	}
};


class Tokenizer {
public:
	Tokenizer(std::istream* is) : is_(is) {}

	void NextToken();
	
	void Reset();

	int CurTok() { return current_token_; }

private:
	void NextChar();

	int NextTokenPass();

	std::istream& get_is() { return *is_; }

	int PreparseIdentifier();

	int EatSpace();

	int PreparseNumber();
	
	std::string identifier_;
	std::istream* is_;
	int line_cnt_ = 0;
	int current_token_ = 0;
	double double_ = 0.0;
	int64_t bigint_ = 0ll;
	int c = ' '; // last char
};


class Parser {
public:
	void ParseExpression(std::istream& is) {
		Tokenizer tok(&is);
		tok_.reset(new Tokenizer(&is));

	}


	void Binop() {

	}

	void Primary() {
		
	}

	std::shared_ptr<Tokenizer> tok_;
};
