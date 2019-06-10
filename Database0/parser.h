#pragma once

#include <vector>
#include <iostream>

#include "token.h"
#include "oracle.h"
#include "attribute.h"

struct TableClause {
	std::string db_name;
	std::string name;
	std::string alias;
	std::map<std::string, int> column_bindings;
	std::vector<uint64_t>rows;
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

struct UnqiueTable {
	std::string db;
	std::string name;
	bool is_alias;
	bool operator==(const UnqiueTable& r) const {

		if (db == r.db) {
			if (name == r.name) {
				return true;
			}
		}
		return false;
	}

	bool operator<(const UnqiueTable& r) const {
		if (db < r.db) {
			return true;
		}
		if (db > r.db) {
			return false;
		}
		return name < r.name;
	}


};

struct QueryColumn {
	ColumnInfo col;
	Attribute* attrib;
};

struct QueryVariable {
	QueryVariable(Attribute* _attrib)
		: attrib(_attrib) {}

	enum IndexMethod : char {
		kDirectIndex,
		kPrimaryRangeIndex
	};

	Attribute* attrib;
	IndexMethod index_method;
	
	std::shared_ptr<ISQLData> index_target;
};


struct QueryClause {
	int type;
	int type_spec;
	std::string default_db_name;
	std::vector<TableClause> vectables;
	std::string insert_table;
	std::string create_table;
	std::string create_index;
	ColumnName index;
	std::vector<ColumnName> insert_col;
	std::vector<std::shared_ptr<ISQLData>> insert_data;
	std::vector<ColumnName> select;
	std::vector<ColumnInfo> create;
	
	std::vector<QueryVariable> variable_attribs;
	std::vector<int> select_result;

	std::map<UnqiueTable, TableClause*> tables;

	std::map <std::string, std::vector<TableJoinClause>> joins;

	std::shared_ptr<IOracle> where_oracle;
	std::vector<std::shared_ptr<VariableOracle>> variables;
	std::vector<std::shared_ptr<FunctionOracle>> functions;
};






class Parser {
public:
	void ParseExpression(const char* txt) {
		std::stringstream ss;
		ss << txt;
		ParseExpression(ss);
	}

	void ParseExpression(std::istream& is);

	void From();

	void Select();

	void Insert();

	void Create();

	void Use();

	bool complete() { return complete_; }

	std::shared_ptr<IOracle> Where();

	////////////////////////////////////////////////
	///// Where clause parser & her friends    /////
	////////////////////////////////////////////////

	// :: = primary ('+' primary)
	std::shared_ptr<IOracle> WhereTopLevel();

	// :: = number
	// :: = identifier/function
	// :: = string
	std::shared_ptr<IOracle> WherePrimary();

	// :: = '(' toplevel ')'
	std::shared_ptr<IOracle> WhereParen();

	// :: = ('+' primary) *
	std::shared_ptr<IOracle> WhereBinopRHS(int expr_prec, std::shared_ptr<IOracle> lhs);

	// :: = '!' primary
	std::shared_ptr<IOracle> WhereUnary();

	// :: = number (0-9.)+
	std::shared_ptr<IOracle> WhereNumber();

	// :: = string (^')+
	std::shared_ptr<IOracle> WhereString();

	// :: = identifier.identifier.*         -- variable
	// :: = identifier(primary, primary, *) -- function
	std::shared_ptr<IOracle> WhereVariable();

	////////////////////////////////////////////////
	///// Utils                                /////
	////////////////////////////////////////////////

	int CurTok() {
		return tok_->CurTok();
	}

	void NextToken() {
		return tok_->NextToken();
	}

	template<typename Arg, typename... Args>
	void Error(Arg arg, Args... args) {
		(*error_) << arg;
		Error(args...);
	}

	template<typename Arg>
	void Error(Arg arg) {
		(*error_) << arg;
	}

	QueryClause target;
	std::shared_ptr<Tokenizer> tok_;
	std::ostream* error_;
	bool complete_ = false;
};
