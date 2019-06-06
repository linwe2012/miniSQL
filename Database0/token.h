#pragma once
#include <string>
#include <set>
#include <map>

// V/D(name, literal, precedence)
#define OPERATOR_LIST(V, D /* acceptable alias */)\
/* Logical operator */                            \
V(And, "and", 5)                                  \
D(And, "&&", 5)                                   \
V(Or, "or", 4)                                    \
D(Or, "||", 4)                                    \
V(Not, "not", 0)                                  \
D(Not, "!", 0)                                    \
/* Compare operators sorted by precedence. */     \
V(Equal, "=", 9)                                  \
D(Equal, "==", 9)                                 \
V(NotEqual, "<>", 9)                              \
D(NotEqual, "!=", 9)                              \
V(Less, "<", 10)                                  \
V(Greater, ">", 10)                               \
V(LessEq, "<=", 10)                               \
V(GreaterEq, ">=", 10)                            \
/*Computational operators*/                       \
V(Add, "+", 12)                                   \
V(Sub, "-", 12)                                   \
V(Mul, "*", 13)                                   \
V(Div, "/", 13)                                   \


#define PUNCTUATER_LIST(V) \
V(LParen, "(", 0)          \
V(RParen, ")", 0)          \
V(SemiColon, ";", 0)


#define KEY_WORD_LIST(V) \
V(Select, "select", 0)   \
V(Insert, "insert", 0)   \
V(Create, "create", 0)   \
V(Where, "where", 0)     \
V(From, "from", 0)       \


class Token {
public:
#define DO_NOTHING(...)
#define ENUM_TOKENS(Name, literal, prec) k##Name,
	enum {
		kUnkown = -9999,
		kIdentifier = -1000,

		KEY_WORD_LIST(ENUM_TOKENS)

		OPERATOR_LIST(ENUM_TOKENS, DO_NOTHING)

		PUNCTUATER_LIST(ENUM_TOKENS)

		kDouble,
		kBigInt,

	};
#undef ENUM_TOKENS
#undef DO_NOTHING
	

	static bool IsOperator(int tok) {
		return tok >= kAnd && tok <= kDiv;
	}

	static bool IsOperatorChar(int c) {
		return operator_char_.count(c);
	}

	static int TestLiteralLower(const std::string& str, int fallback = kUnkown) {
		std::string lower = ToLower(str);
		auto is_tok = tokens_.find(lower);
		if (is_tok == tokens_.end()) {
			return is_tok->second;
		}
		return fallback;
	}

	static std::string ToLower(const std::string& s);

	static int Precedence(int i) {
		auto itr = tok_precedence_.find(i);
		if (itr == tok_precedence_.end()) {
			return -1;
		}
		return itr->second;
	}

private:
	static std::set<int> operator_char_;
	static std::map<std::string, int> operator_tokens_;
	static std::map<std::string, int> keywords_;
	static std::map<std::string, int> tokens_;
	static std::map<int, int> tok_precedence_;
};
