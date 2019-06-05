#pragma once
#include "buffer-manager.h"
#include "sqldata.h"
#include "catalog-manager.h"

struct Memory {
	void* data;
	size_t size;
};

class ITernaryOracle {
public:
	virtual bool Test(const Memory a, const Memory b, const Memory c) = 0;
};

class IBinaryOracle {
public:
	virtual bool Test(const Memory a, const Memory b) = 0;
};

class IUnaryOracle {
public:
	virtual bool Test(const Memory a) = 0;
};

class SQLEqVisitor : public IConstSQLDataVisitor {
public:
	bool result() {
		return result_;
	}

	void Bind(const void* data) {
		data_ = data;
	}

	void Visit(const SQLBigInt* i) override {
		if (data_ == nullptr) {
			result_ = false;
		}
		else {
			result_ = (*(SQLBigInt::CType*)(data_) == i->Value());
		}
	}

	void Visit(const SQLDouble* i) override {
		if (data_ == nullptr) {
			result_ = false;
		}
		else {
			result_ = (*(SQLDouble::CType*)(data_) == i->Value());
		}
		
	}

	void Visit(const SQLString* i) override {
		if (data_ == nullptr) {
			result_ = false;
		}
		else {
			result_ = (strcmp((const char*)data_, i->Value()) == 0);
		}
	}

	void Visit(const SQLNull* i) override {
		if (data_ == nullptr) {
			result_ = true;
		}
		else {
			result_ = false;
		}
	}

	void Visit(const SQLTimeStamp* i) override {
		if (data_ == nullptr) {
			result_ = false;
		}
		else {
			result_ = *((SQLTimeStampStruct*)data_) == i->Value();
		}
	}

	const void* data_;
	bool result_;
};


class EqualOracle : public IUnaryOracle {
public:
	bool Test(const Memory a) override {
		visitor.Bind(a.data);
		target->Accept(&visitor);
		return visitor.result();
	}


	ISQLData* target;
	SQLEqVisitor visitor;
};

enum struct Operator {
	kAnd = -99, kOr, kNot, // to lazy to create a special unary operator
	kEqual, kAdd, kMinus, kMul, kDiv
};

class BinopAST;

class IAST {
public:
	virtual BinopAST* AsBinopAST() { return nullptr; }
};

struct BinopAST : public IAST {
public:
	BinopAST* AsBinopAST() { return this; }

	bool IsLogical() {
		return op_ >= Operator::kAnd && op_ <= Operator::kNot;
	}

	bool IsComputational() {
		return op_ >= Operator::kEqual && op_ <= Operator::kDiv;
	}

	IAST* lhs() {
		return lhs_;
	}

	IAST* rhs() {
		return rhs_;
	}

	Operator op() {
		return op_;
	}

private:
	Operator op_;
	IAST* lhs_;
	IAST* rhs_;
};

class AttributeAST : public IAST {
public:


	ColumnName column;
	BufferManager::Iterator<char> itr;
};



class WhereClause {

	struct ConjunctiveNF {
		std::vector<IUnaryOracle*> unaries;
		std::vector<IBinaryOracle*> binaries;
		std::vector<ITernaryOracle*> ternaries;
	};

	IAST* Normalize(IAST* target) {
		auto binop = target->AsBinopAST();
		if (!binop || !binop->IsLogical()) {
			return target;
		}

		IAST* lhs = binop->lhs();
		IAST* rhs = binop->rhs();
		if (lhs != nullptr) {
			lhs = Normalize(binop->lhs());
		}

		if (binop->rhs() != nullptr) {
			rhs = Normalize(binop->rhs());
		}

		switch (binop->op())
		{
		case Operator::kAnd:
			return NormalizeAnd(binop, lhs, rhs);
		case Operator::kOr:
		case Operator::kNot:

		default:
			break;
		}
	}

	IAST* NormalizeAnd(BinopAST* target, IAST* lhs, IAST* rhs) {

	}

	std::vector<ConjunctiveNF> dnf_; // disjunctive normal form
};

void Filter(IUnaryOracle* oracle, BufferManager::Iterator<char>begin, BufferManager::Iterator<char>end) {
	Memory nil{ nullptr, 0 };
	if (begin.IsNil()) {
		oracle->Test(nil);
	}

}