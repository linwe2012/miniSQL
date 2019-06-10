#pragma once
#include <string>
#include <set>
#include <map>
#include <algorithm>
#include <sstream>
#include <functional>

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


#define KEY_WORD_LIST(V, D) \
V(Select, "select", 0)   \
V(Insert, "insert", 0)   \
V(Create, "create", 0)   \
V(Where, "where", 0)     \
V(From, "from", 0)       \
V(Values, "values", 0)   \
D(Values, "value", 0)    \
V(Into, "into", 0)   \
V(Table, "table", 0) \
V(Comment, "comment", 0) \
V(Null, "null", 0)\
V(TypeVarchar, "varchar", 0)\
V(TypeDouble, "double", 0) \
V(TypeSingle, "single", 0) \
V(TypeInt, "integer", 0)\
V(TypeFloat, "float", 0)\
D(TypeInt, "int", 0)\
V(Primary, "primary", 0)\
V(Key, "key", 0)\
V(Use, "use", 0)\
V(Index, "index", 0)\
V(ExecFile, "execfile", 0)\
V(Unique, "unique", 0)

#define KEY_BUILTIN_FUNC(V, D) \
V(CurDate, "getdate", 0)       \
D(CurDate, "current_date", 0)  \
V(ToDate, "to_date", 0)        \


class Token {
public:
#define DO_NOTHING(...)
#define ENUM_TOKENS(Name, literal, prec) k##Name,
	enum {
		kUnkown = -9999,
		kIdentifier = -1000,

		KEY_WORD_LIST(ENUM_TOKENS, DO_NOTHING)

		OPERATOR_LIST(ENUM_TOKENS, DO_NOTHING)

		PUNCTUATER_LIST(ENUM_TOKENS)

		KEY_BUILTIN_FUNC(ENUM_TOKENS, DO_NOTHING)

		kDouble,
		kBigInt,
		kString,

		kEof = -1,
	};
#undef ENUM_TOKENS
#undef DO_NOTHING
	

	static bool IsOperator(int tok) {
		return tok >= kAnd && tok <= kDiv;
	}

	static bool IsBinaryop(int tok) {
		return IsOperator(tok) && (tok != kNot);
	}

	static bool IsUnary(int tok) {
		return tok == kNot;
	}

	static bool IsCompare(int tok) {
		return tok >= kEqual && tok <= kGreaterEq;
	}

	static bool IsOperatorChar(int c) {
		return operator_char_.count(c);
	}

	static int TestLiteralLower(const std::string& str, int fallback = kUnkown) {
		std::string lower = ToLower(str);
		auto is_tok = tokens_.find(lower);
		if (is_tok == tokens_.end()) {
			return fallback;
		}
		return is_tok->second;
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


class Tokenizer {
public:
	Tokenizer(std::istream* is) : is_(is) {}

	void NextToken();

	void Reset();

	int CurTok() { return current_token_; }

	void Bind(std::istream* is) {
		if (is_ != nullptr) {
			delete is_;
		}
		is_ = is;
	}

	std::string identifier() {
		return identifier_;
	}

	double num_double() {
		return double_;
	}

	int64_t num_bigint() {
		return bigint_;
	}

	std::string string() {
		return string_;
	}

private:
	void NextChar();

	int NextTokenPass();

	std::istream& get_is() { return *is_; }

	int PreparseIdentifier();

	int EatSpace();

	int PreparseNumber();

	std::string identifier_;
	std::string string_;
	std::istream* is_;
	std::function<void(const char*)> error_;
	int line_cnt_ = 0;
	int current_token_ = 0;
	double double_ = 0.0;
	int64_t bigint_ = 0ll;
	int c = ' '; // last char
};

std::vector<std::string> Explode(const std::string& s, char c);