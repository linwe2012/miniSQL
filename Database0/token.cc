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
		KEY_WORD_LIST(MAP_TOKENS)
};

std::map<std::string, int> Token::tokens_{
	OPERATOR_LIST(MAP_TOKENS, MAP_TOKENS)
	KEY_WORD_LIST(MAP_TOKENS)
	PUNCTUATER_LIST(MAP_TOKENS)
};

#undef MAP_TOKENS


std::string Token::ToLower(const std::string& s) {
	std::string res;
	std::transform(s.begin(), s.end(), res, [](const char a) -> char {
		return static_cast<char>(tolower(a));
	});
	return res;
}