#include "parser.h"

void Parser::From() {
	while (true)
	{
		if (CurTok() != Token::kIdentifier) {
			return;
		}

		std::string tb = tok_->identifier();
		std::string alias;

		std::string

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


		if (CurTok() != ',') {
			break;
		}

		NextToken(); // eat ','
	}
}

std::shared_ptr<IOracle> Parser::WhereTopLevel() {
	auto lhs = WherePrimary();

	if (!lhs) {
		NextToken();
		return std::make_shared<IOracle>(nullptr);
	}

	switch (CurTok())
	{
	case '(': return WhereParen();
	case Token::kUnkown:
		Error("Unkown token: ", tok_->identifier());
		NextToken();
		break;
	default:
		break;
	}
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

	default:
		break;
	}
}

std::shared_ptr<IOracle> Parser::WhereParen() {
	auto top = WhereTopLevel();
	if (CurTok() != ')') {
		Error("Expected ')' parenthese to match '('");
		return std::make_shared<IOracle>(nullptr);
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
			return std::make_shared<IOracle>(nullptr);
		}

		int next_prec = Token::Precedence(CurTok());

		// lhs + (rhs * rhs...)
		if (tok_prec < next_prec) {
			rhs = WhereBinopRHS(tok_prec + 1, rhs);
			if (!rhs) {
				return std::make_shared<IOracle>(nullptr);
			}
		}

		// (lhs + rhs) ...
		lhs = std::make_shared<IOracle>(new BinopOracle(binop, lhs, rhs));
	}
}

std::shared_ptr<IOracle> Parser::WhereUnary() {
	int unary = CurTok();
	NextToken(); // eat unary operator
	auto lhs = WherePrimary();
	if (!lhs) {
		return std::make_shared<IOracle>(nullptr);
	}

	return std::make_shared<IOracle>(new BinopOracle(unary, lhs));
}

std::shared_ptr<IOracle> Parser::WhereNumber() {
	int cur = CurTok();
	NextToken();
	switch (cur)
	{
	case Token::kDouble:
		return std::make_shared<IOracle>(
			new ConstantDataOracle(
				std::make_shared<ISQLData>(new SQLDouble(tok_->num_double()))
			)
			);

	case Token::kBigInt:
		return std::make_shared<IOracle>(
			new ConstantDataOracle(
				std::make_shared<ISQLData>(new SQLBigInt(tok_->num_bigint()))
			)
			);
	}
}

std::shared_ptr<IOracle> Parser::WhereString() {
	NextToken();
	return std::make_shared<IOracle>(
		new ConstantDataOracle(
			std::make_shared<ISQLData>(new SQLString(tok_->string()))
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
		auto func = std::make_shared<FunctionOracle>(new FunctionOracle(identifier, params));
		target.functions.push_back(func);
		NextToken();
		return func;
	}

	auto var = std::make_shared<VariableOracle>(new VariableOracle(identifier));
	target.variables.push_back(var);
	return var;
}

