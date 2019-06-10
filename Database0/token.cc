#include "token.h"
#include <algorithm>

std::set<int> Token::operator_char_{
	int('+'),
	int('-'),
	int('*'),
	int('/'),

	int('|'),
	int('&'),
	int('!'),

	int('>'),
	int('<'),
	int('='),
};


#define MAP_TOKENS(Name, literal, prec) {std::string(literal), Token::k##Name},
std::map<std::string, int> Token::operator_tokens_{
	OPERATOR_LIST(MAP_TOKENS, MAP_TOKENS)
};

std::map<std::string, int> Token::keywords_{
	KEY_WORD_LIST(MAP_TOKENS, MAP_TOKENS)
};

std::map<std::string, int> Token::tokens_{
	OPERATOR_LIST(MAP_TOKENS, MAP_TOKENS)
	KEY_WORD_LIST(MAP_TOKENS, MAP_TOKENS)
	PUNCTUATER_LIST(MAP_TOKENS)
};
#undef MAP_TOKENS


#define MAP_PREC(Name, literal, prec) { Token::k##Name, prec },
#define DO_NOTHING(...) 
std::map<int, int> Token::tok_precedence_{
	OPERATOR_LIST(MAP_PREC, DO_NOTHING)
	KEY_WORD_LIST(MAP_PREC, DO_NOTHING)
	PUNCTUATER_LIST(MAP_PREC)
};
#undef MAP_PREC
#undef DO_NOTHING


std::string Token::ToLower(const std::string& s) {
	std::string res(s);
	std::transform(s.begin(), s.end(), res.begin(), [](const char a) -> char {
		return static_cast<char>(tolower(a));
	});
	return res;
}



///////////////////////////////////
///// Tokenizer 
//////////////////////////////////

int Tokenizer::NextTokenPass() {
	EatSpace();

	if (isalpha(c)) {
		return PreparseIdentifier();
	}

	if (isdigit(c)) {
		return PreparseNumber();
	}

	// string
	if (c == '\'') {
		NextChar();
		while (c != '\'')
		{
			// escape sequence
			if (c == '\\') {
				NextChar();
				if (c == -1) {
					error_("Unexpected end of stream");
					return Token::kEof;
				}
				switch (c)
				{
				case '\'': string_ += '\'';	 break;
				case 'n': string_ += '\n';	 break;
				case 'r': string_ += '\r';	 break;
				case 't': string_ += '\t';	 break;
				case '\\': string_ += '\\';	 break;
				case '"': string_ += '"'; break;
				default:
					string_ += '\\';
					string_ += c;
					error_("Unkown escape in string");
					break;
				}
			}
			else {
				string_ += c;
			}
			NextChar();
			if (c == -1) {
				error_("Unexpected end of stream");
				return Token::kEof;
			}
		}
		NextChar(); // eat '
		return Token::kString;
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

	int res = c;
	NextChar();
	return res;
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
	while (isalnum(c) || c == '_' || c == '.')
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
	return 0;
}

int Tokenizer::PreparseNumber()
{
	std::stringstream ss;
	int double_flag = (c == '.');
	ss << (char)c;
	NextChar();
	while (isdigit(c) || c == '.')
	{
		if (c == '.') {
			double_flag = 1;
		}
		ss << (char)c;
		NextChar();
	}
	if (double_flag) {
		ss >> double_;
		return Token::kDouble;
	}
	ss >> bigint_;
	return Token::kBigInt;
}

std::vector<std::string> Explode(const std::string& s, char c) {
	std::string buff{ "" };
	std::vector<std::string> v;

	for (auto n : s)
	{
		if (n != c)
		{
			buff += n;
		}
		else if (n == c && buff != "") {
			v.push_back(std::move(buff));
			buff.clear();
		}
	}
	if (buff != "")
	{
		v.push_back(std::move(buff));
	}

	return v;
}