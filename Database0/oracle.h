#pragma once
#include "buffer-manager.h"
#include "sqldata.h"
#include "catalog-manager.h"
#include "token.h"

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



class BinopOracle;

class IOracle {
public:
	using Iterator = BufferManager::Iterator<char>;
	using Iterators = std::vector<BufferManager::Iterator<char>>;
	
	virtual bool Test(Iterators& itrs) = 0;
	virtual std::shared_ptr<ISQLData> Data(Iterators& itrs) = 0;
	
	virtual BinopOracle* AsBinopAST() { return nullptr; }
	virtual bool Boolean() = 0;

	
};


struct BinopOracle : public IOracle {
public:
	BinopOracle(Operator op, IOracle* lhs, IOracle* rhs)
		: op_(op), lhs_(lhs), rhs_(rhs) {}

	BinopOracle(Operator op, IOracle* lhs)
		: op_(op), lhs_(lhs), rhs_(nullptr) {}

	BinopOracle* AsBinopAST() { return this; }

	bool IsLogical() {
		return op_ >= Operator::kAnd && op_ <= Operator::kNot;
	}

	bool IsComputational() {
		return op_ >= Operator::kEqual && op_ <= Operator::kDiv;
	}

	IOracle* lhs() { return lhs_; }

	IOracle* rhs() { return rhs_; }

	Operator op() { return op_; }

	bool Test(Iterators& itrs) override;

	std::shared_ptr<ISQLData> Data(Iterators& itrs) override;

private:
	Operator op_;
	IOracle* lhs_;
	IOracle* rhs_;
};


class VariableOracle : public IOracle {
	void Bind(int id) { id_ = id; }
	Iterator& Itr(Iterators& itrs) {
		return itrs[id_];
	}

	bool Test(Iterators& itrs) override {
		if (type_ == SQLTypeID<SQLString>::value) {
			return Itr(itrs).Cast<char*>().IsNil();
		}
		return Itr(itrs).IsNil();
	}

	std::shared_ptr<ISQLData> Data(Iterators& itrs) override;

private:
	int id_;
	int type_;
};

class RangedDataOracle : public IOracle {
};

class ConstantDataOracle : public IOracle {


	std::shared_ptr<ISQLData> constant_;
};





class WhereClause {

	struct ConjunctiveNF {
		std::vector<IUnaryOracle*> unaries;
		std::vector<IBinaryOracle*> binaries;
		std::vector<ITernaryOracle*> ternaries;
	};

	IOracle* Normalize(IOracle* target) {
		auto binop = target->AsBinopAST();
		if (!binop || !binop->IsLogical()) {
			return target;
		}

		IOracle* lhs = binop->lhs();
		IOracle* rhs = binop->rhs();
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

	IOracle* NormalizeAnd(BinopOracle* target, IOracle* lhs, IOracle* rhs) {

	}

	void TraverseAST() {

	}

	
	std::vector<ConjunctiveNF> dnf_; // disjunctive normal form
};




void Filter(IUnaryOracle* oracle, BufferManager::Iterator<char>begin, BufferManager::Iterator<char>end) {
	Memory nil{ nullptr, 0 };
	if (begin.IsNil()) {
		oracle->Test(nil);
	}

}