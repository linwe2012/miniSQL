#include "parser.h"

int Tokenizer::NextTokenPass() {
	EatSpace();

	if (isalpha(c)) {
		return PreparseIdentifier();
	}

	if (isdigit(c)) {
		return PreparseNumber();
	}

	if (Token::IsOperatorChar(c)) {
		identifier_ = c;
		NextChar();

		while (Token::IsOperatorChar(c))
		{
			identifier_ += c;
			NextChar();
		}
		return Token::TestLiteralLower(identifier_, Token::kUnkown);
	}

	return c;
}

void Tokenizer::NextToken() {
	current_token_ = NextTokenPass();
}

void Tokenizer::NextChar() {
	std::istream& is = get_is();
	c = is.get();
}

void Tokenizer::Reset() {
	c = ' ';
	identifier_.clear();
	line_cnt_ = 0;
}

int Tokenizer::PreparseIdentifier() {
	identifier_ = c;
	NextChar();
	while (isalpha(c))
	{
		identifier_ += c;
		NextChar();
	}
	return Token::TestLiteralLower(identifier_, Token::kIdentifier);
}

int Tokenizer::EatSpace() {
	int flag_next_line = ' ';
	while (isspace(c))
	{
		if ((flag_next_line == ' ' || flag_next_line == c) && c == '\n' || c == '\r') {
			flag_next_line = c;
			++line_cnt_;
		}
		else {
			flag_next_line = ' ';
		}
		NextChar();
	}
}

int Tokenizer::PreparseNumber()
{
	std::stringstream ss;
	int double_flag = (c == '.');
	ss << c;
	NextChar();
	while (isdigit(c))
	{
		if (c == '.') {
			double_flag = 1;
		}
		ss << c;
		NextChar();
	}
	if (double_flag) {
		ss >> double_;
		return Token::kDouble;
	}
	ss >> bigint_;
	return Token::kBigInt;
}
