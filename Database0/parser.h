#pragma once

#include <vector>


#include "token.h"
#include "oracle.h"
#include "attribute.h"

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

struct UnqiueTable {
	std::string db;
	std::string name;
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

struct QueryClause {
	int type;
	std::string default_db_name;
	std::vector<TableClause> vectables;
	std::vector<ColumnName> select;
	std::vector<ColumnInfo> create;
	
	std::map<UnqiueTable, TableClause*> tables;
	std::map < std::string, std::vector<TableJoinClause>> joins;
	
	std::shared_ptr<IOracle> where_oracle;
	std::vector< std::shared_ptr<VariableOracle>> variables;
	std::vector< std::shared_ptr<FunctionOracle>> functions;
};






class Parser {
public:
	void ParseExpression(std::istream& is) {
		Tokenizer tok(&is);
		tok_.reset(new Tokenizer(&is));
		NextToken();
		int cur = CurTok();
		switch (cur)
		{
		case Token::kInsert: // fall through
		case Token::kSelect: // fall through
		case Token::kCreate:
			target.type = CurTok();
			break;
		case Token::kWhere:
			Where();
			break;
		case Token::kFrom:
			From();
		default:
			Error("Unexpected token");
			break;
		}
	}

	void From();
	////////////////////////////////////////////////
	///// Where clause parser & her friends    /////
	////////////////////////////////////////////////

	std::shared_ptr<IOracle> Where() {
		return WhereTopLevel();
	}

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
};
