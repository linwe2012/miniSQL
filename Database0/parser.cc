#include "parser.h"

void Parser::ParseExpression(std::istream& is) {
	tok_.reset(new Tokenizer(&is));
	NextToken();
	if (error_ == nullptr) {
		error_ = &std::cout;
	}

	while (CurTok() != Token::kEof)
	{
		int cur = CurTok();
		switch (cur)
		{
		case Token::kInsert:
			target.type = CurTok();
			Insert();
			break;
		case Token::kSelect:
			target.type = CurTok();
			Select();
			break;
		case Token::kCreate:
			target.type = CurTok();
			Create();
			break;
		case Token::kWhere:
			target.where_oracle = Where();
			break;
		case Token::kFrom:
			From();
			break;
		case Token::kUse:
			target.type = CurTok();
			Use();
			break;
		case Token::kEof:
			break;
		case Token::kExecFile:
			target.type = CurTok();
			NextToken();
			return;
		case ';':
			complete_ = true;
			return;
		default:
			Error("Unexpected token");
			NextToken();
			break;
		}
	}

}

void Parser::From() {
	NextToken();
	while (true)
	{
		if (CurTok() != Token::kIdentifier) {
			return;
		}

		std::string tb = tok_->identifier();
		std::string alias;
		NextToken();

		if (CurTok() == Token::kIdentifier) {
			alias = tok_->identifier();
			NextToken();
		}

		target.vectables.push_back(TableClause{ "", tb, alias });

		if (CurTok() != ',') {
			break;
		}

		NextToken(); // eat ','
	}
}

void Parser::Select()
{
	NextToken();
	while (CurTok() == Token::kIdentifier || CurTok() == Token::kMul)
	{
		ColumnName col;
		if (CurTok() == Token::kMul) {
			col.column_name = '*';
		}
		else {
			col.column_name = tok_->identifier();
		}
		NextToken();

		if (CurTok() == Token::kIdentifier) {
			col.alias = tok_->identifier();
			NextToken();
		}

		target.select.push_back(col);

		if (CurTok() != ',') {
			break;
		}

		NextToken(); // eat ','
	}
}

void Parser::Insert()
{
	NextToken();
	bool resume = true;
	bool value = false;
	while (resume)
	{
		if (CurTok() == Token::kInto) {
			NextToken();
			if (CurTok() != Token::kIdentifier) {
				Error("Expected indentifier after into");
				break;
			}
			target.insert_table = tok_->identifier();
			NextToken();
		}

		else if (CurTok() == Token::kValues) {
			value = true;
			NextToken();
		}

		else if (CurTok() == '(') {
			NextToken();
			if (!value) {
				while (CurTok() == Token::kIdentifier)
				{
					target.insert_col.push_back(ColumnName{ "", "", tok_->identifier() });
					NextToken();
					if (CurTok() == ')') {
						break;
					}
					if (CurTok() != ',') {
						Error("Expected seperator ','");
					}
					NextToken();
				}
			}
			else
			{
				IOracle::Iterators itrs;
				while (CurTok() != ')')
				{
					target.insert_data.push_back(WherePrimary()->Data(itrs));
					if (CurTok() == ')') {
						break;
					}
					if (CurTok() != ',') {
						Error("Expected seperator ','");
					}
					NextToken();
				}
			}

			if (CurTok() != ')') {
				Error("Expected ')' to close parenthese");
			}

			NextToken();
		}
		else {
			break;
		}
	}
}

void Parser::Create()
{
	NextToken();
	if (CurTok() == Token::kTable) {
		target.type_spec = Token::kTable;
		NextToken();

		if (CurTok() != Token::kIdentifier) {
			Error("Expected Identifier as table name");
			return;
		}

		target.create_table = tok_->identifier();
		NextToken();
		if (CurTok() != '(') {
			Error("Expected '(' ");
			return;
		}
		
		NextToken();
		bool bailout = false;
		while (CurTok() == Token::kIdentifier)
		{
			ColumnInfo info;
			info.name.column_name = tok_->identifier();
			info.type = -1;
			info.is_primary_key = false;
			info.max_length = -1;
			info.nullable = true;
			
			NextToken();
			while (CurTok() != ',')
			{
				switch (CurTok())
				{
				case Token::kNot:
					NextToken();
					if (CurTok() != Token::kNull) {
						Error("Unexpected token, did you mean not null?");
						return;
					}
					info.nullable = false;
					break;
				case Token::kPrimary:
					NextToken();
					if (CurTok() != Token::kKey) {
						Error("Unexpected token, did you mean primary key?");
						return;
					}
					info.is_primary_key = true;
					break;
				case Token::kComment:
					NextToken();
					if (CurTok() != Token::kString) {
						Error("Expected string as comment");
					}
					info.comment = tok_->string();
					break;
				case Token::kTypeDouble:
				case Token::kTypeSingle:
				case Token::kTypeFloat:
					info.type = SQLTypeID<SQLDouble>::value;
					break;
				case Token::kTypeInt:
					info.type = SQLTypeID<SQLBigInt>::value;
					break;
				case Token::kTypeVarchar:
					info.type = SQLTypeID<SQLString>::value;
					break;
				case Token::kUnique:
					//todo
					break;
				default:
					bailout = true;
					break;
				}
				
				if (bailout) {
					bailout = false;
					break;
				}
				NextToken();
			}
			if (info.type < 0) {
				info.type = SQLTypeID<SQLBigInt>::value;
				Error("Expected type specification, int assumed");
			}
			target.create.push_back(info);
			if (CurTok() == ')') {
				break;
			}
			NextToken();
		}

		if (CurTok() != ')') {
			Error("Expected ')' ");
			return;
		}
		NextToken();
		return;
	}
	if (CurTok() == Token::kIndex) {
		target.type_spec = Token::kIndex;
		NextToken();

		if (CurTok() != Token::kIdentifier) {
			Error("Expected Identifier as table name");
			return;
		}
		target.create_index = tok_->identifier();
		NextToken();

		if (CurTok() != Token::kIdentifier || tok_->identifier() != "on") {
			Error("Expected Identifier on");
			return;
		}
		NextToken();
		if (CurTok() != Token::kIdentifier) {
			Error("Expected Identifier as table name");
			return;
		}
		target.index.table_name = tok_->identifier();
		NextToken();
		if (CurTok() != '(') {
			Error("Expected '('");
			return;
		}
		NextToken();
		if (CurTok() != Token::kIdentifier) {
			Error("Expected identifier as column name");
			return;
		}
		target.index.column_name = tok_->identifier();
		NextToken();
		if (CurTok() != ')') {
			Error("Expected ')' to close parenthese");
			return;
		}
		NextToken();
	}
}

void Parser::Use()
{
	NextToken();
	if (CurTok() != Token::kIdentifier) {
		Error("Expected identifier as database name");
		return;
	}

	target.default_db_name = tok_->identifier();
	NextToken();
}

std::shared_ptr<IOracle> Parser::Where() {
	NextToken();
	return WhereTopLevel();
}

std::shared_ptr<IOracle> Parser::WhereTopLevel() {
	if (CurTok() == '(') {
		return WhereParen();
	}

	auto lhs = WherePrimary();
	if (!lhs) {
		NextToken();
		return std::shared_ptr<IOracle>(nullptr);
	}

	return WhereBinopRHS(0, lhs);
}

std::shared_ptr<IOracle> Parser::WherePrimary() {
	if (Token::IsUnary(CurTok())) {
		return WhereUnary();
	}

	switch (CurTok())
	{
	case Token::kDouble: // fall through
	case Token::kBigInt:
		return WhereNumber();
	case Token::kString:
		return WhereString();
	case Token::kIdentifier:
		return WhereVariable();
	default:
		break;
	}
}

std::shared_ptr<IOracle> Parser::WhereParen() {
	NextToken(); // eat '('
	auto top = WhereTopLevel();
	if (CurTok() != ')') {
		Error("Expected ')' parenthese to match '('");
		return std::shared_ptr<IOracle>(nullptr);
	}
	NextToken();
	return top;
}

// :: = ('+' primary) *

std::shared_ptr<IOracle> Parser::WhereBinopRHS(int expr_prec, std::shared_ptr<IOracle> lhs) {
	while (1)
	{
		int tok_prec = Token::Precedence(CurTok());

		// If this is a binop that binds at least as tightly as the current binop,
		// consume it, otherwise we are done.
		if (tok_prec < expr_prec) {
			return lhs;
		}

		int binop = CurTok();

		NextToken();

		auto rhs = WherePrimary();
		if (!rhs) {
			return std::shared_ptr<IOracle>(nullptr);
		}

		int next_prec = Token::Precedence(CurTok());

		// lhs + (rhs * rhs...)
		if (tok_prec < next_prec) {
			rhs = WhereBinopRHS(tok_prec + 1, rhs);
			if (!rhs) {
				return std::shared_ptr<IOracle>(nullptr);
			}
		}

		// (lhs + rhs) ...
		lhs = std::shared_ptr<IOracle>(new BinopOracle(binop, lhs, rhs));
	}
}

std::shared_ptr<IOracle> Parser::WhereUnary() {
	int unary = CurTok();
	NextToken(); // eat unary operator
	auto lhs = WherePrimary();
	if (!lhs) {
		return std::shared_ptr<IOracle>(nullptr);
	}

	return std::shared_ptr<IOracle>(new BinopOracle(unary, lhs));
}

std::shared_ptr<IOracle> Parser::WhereNumber() {
	int cur = CurTok();
	std::shared_ptr<IOracle> res;
	switch (cur)
	{
	case Token::kDouble:
		res =  std::shared_ptr<IOracle>(
			new ConstantDataOracle(
				std::shared_ptr<ISQLData>(new SQLDouble(tok_->num_double()))
			)
			);

	case Token::kBigInt:
		res = std::shared_ptr<IOracle>(
			new ConstantDataOracle(
				std::shared_ptr<ISQLData>(new SQLBigInt(tok_->num_bigint()))
			)
			);
	}
	NextToken();
	return res;
}

std::shared_ptr<IOracle> Parser::WhereString() {
	NextToken();
	return std::shared_ptr<IOracle>(
		new ConstantDataOracle(
			std::shared_ptr<ISQLData>(new SQLString(tok_->string()))
		)
		);
}

std::shared_ptr<IOracle> Parser::WhereVariable() {
	std::string identifier = tok_->identifier();

	NextToken();

	if (CurTok() == '(') {
		FunctionOracle::Params params;
		NextToken();
		while (CurTok() != ')')
		{
			params.push_back(WherePrimary());
		}
		auto func = std::shared_ptr<FunctionOracle>(new FunctionOracle(identifier, params));
		target.functions.push_back(func);
		NextToken();
		return func;
	}

	auto var = std::shared_ptr<VariableOracle>(new VariableOracle(identifier));
	target.variables.push_back(var);
	return var;
}

