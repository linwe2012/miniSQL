#pragma once
#include "buffer-manager.h"
#include "sqldata.h"
#include "catalog-manager.h"
#include "token.h"

struct Memory {
	void* data;
	size_t size;
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

class BinopOracle;

class IOracle {
public:
	using Iterator = BufferManager::Iterator<char>;
	using Iterators = std::vector<BufferManager::Iterator<char>>;
	
	virtual bool Test(Iterators& itrs) = 0;
	virtual std::shared_ptr<ISQLData> Data(Iterators& itrs) = 0;
	
	virtual BinopOracle* AsBinopAST() { return nullptr; }
	// virtual bool Boolean() = 0;
};


struct BinopOracle : public IOracle {
public:
	BinopOracle(int op, std::shared_ptr<IOracle> lhs, std::shared_ptr<IOracle> rhs)
		: op_(op), lhs_(lhs), rhs_(rhs) {}

	BinopOracle(int op, std::shared_ptr<IOracle> lhs)
		: op_(op), lhs_(lhs), rhs_(nullptr) {}

	BinopOracle* AsBinopAST() { return this; }

	bool IsLogical() {
		return op_ >= Token::kAnd && op_ <= Token::kNot;
	}

	bool IsComputational() {
		return op_ >= Token::kEqual && op_ <= Token::kDiv;
	}

	std::shared_ptr<IOracle> lhs() { return lhs_; }

	std::shared_ptr<IOracle> rhs() { return rhs_; }

	int op() { return op_; }

	bool Test(Iterators& itrs) override;

	std::shared_ptr<ISQLData> Data(Iterators& itrs) override;

private:
	int op_;
	std::shared_ptr<IOracle> lhs_;
	std::shared_ptr<IOracle> rhs_;
};



class VariableOracle : public IOracle {
public:
	void Bind(int id) { id_ = id; }
	Iterator& Itr(Iterators& itrs) {
		return itrs[id_];
	}

	VariableOracle (std::string name) 
		: name_(name) {}

	bool Test(Iterators& itrs) override {
		if (type_ == SQLTypeID<SQLString>::value) {
			return Itr(itrs).Cast<char*>().IsNil();
		}
		return Itr(itrs).IsNil();
	}

	std::shared_ptr<ISQLData> Data(Iterators& itrs) override;

	const std::string& name() { return name_; }

private:
	int id_;
	int type_;
	std::string name_;
};

struct FunctionPayLoad {
	const std::vector<std::shared_ptr<IOracle>> params;
};

class FunctionOracle : public IOracle {
public:
	using Func   = std::function<void(FunctionPayLoad&)>;
	using Params = std::vector<std::shared_ptr<IOracle>>;

	FunctionOracle(std::string function_name, Params params)
		: function_name_(function_name), params_(params) {}

	std::shared_ptr<ISQLData> Data(Iterators& itrs) override;
	bool Test(Iterators& itrs) override;

private:
	Func func_;
	std::string function_name_;
	Params params_;
};

class RangedDataOracle : public IOracle {
};

class ConstantDataOracle : public IOracle {
public:
	ConstantDataOracle(std::shared_ptr<ISQLData> constant)
		: constant_(constant) {}

	std::shared_ptr<ISQLData> Data(Iterators& itrs) override;
	bool Test(Iterators& itrs) override;
private:
	std::shared_ptr<ISQLData> constant_;
};





