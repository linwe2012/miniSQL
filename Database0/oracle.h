#pragma once
#include "buffer-manager.h"
#include "sqldata.h"
#include "catalog-manager.h"
#include "token.h"

#define SOLID_ORACLE_LIST(V)\
V(BinopOracle)\
V(VariableOracle) \
V(FunctionOracle)\
V(ConstantDataOracle)

#define FWD_DECL_ORACLE(o) class o;
SOLID_ORACLE_LIST(FWD_DECL_ORACLE)
#undef FWD_DECL_ORACLE

#define ORACLE_IMPL(o) \
void Accept(IOracleVisitor* visitor) override;\
o* As##o() override { return this; }

class IOracleVisitor;

class IOracle {
public:
	using Iterator = BufferManager::Iterator<char>;
	using Iterators = std::vector<BufferManager::Iterator<char>>;
	
	virtual bool Test(Iterators& itrs) = 0;
	virtual std::shared_ptr<ISQLData> Data(Iterators& itrs) = 0;
	
	virtual void Accept(IOracleVisitor* visitor) = 0;

#define ORACLE_AS(o) virtual o* As##o() { return nullptr; }
	SOLID_ORACLE_LIST(ORACLE_AS)
#undef ORACLE_AS
};


struct BinopOracle : public IOracle {
public:
	ORACLE_IMPL(BinopOracle)

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

	void set_lhs(std::shared_ptr<IOracle> lhs) { lhs_ = lhs; }

	void set_rhs(std::shared_ptr<IOracle> rhs) { rhs_ = rhs; }

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
	ORACLE_IMPL(VariableOracle)
	VariableOracle (std::string name) 
		: name_(name) {}

	void Bind(int id, int type) { id_ = id; type_ = type; }

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

	const std::string& name() { return name_; }

	int id() { return id_; }

private:
	int id_;
	int type_;
	std::string name_;
};

struct FunctionPayLoad {
	FunctionPayLoad(IOracle::Iterators* itrs, std::vector<std::shared_ptr<IOracle>>* _params)
		: params_(_params) {}

	const std::vector<std::shared_ptr<IOracle>>& params() {
		return *params_;
	}

	std::vector<std::shared_ptr<ISQLData>>& return_vals() {
		return return_vals_;
	}

	IOracle::Iterators& itrs() {
		return *itrs_;
	}
	
private:
	IOracle::Iterators* itrs_;
	const std::vector<std::shared_ptr<IOracle>>* params_;
	std::vector<std::shared_ptr<ISQLData>> return_vals_;
};

class FunctionOracle : public IOracle {
public:
	ORACLE_IMPL(FunctionOracle)

	using Func   = std::function<void(FunctionPayLoad&)>;
	using Params = std::vector<std::shared_ptr<IOracle>>;

	FunctionOracle(std::string function_name, Params params)
		: function_name_(function_name), params_(params) {}

	std::shared_ptr<ISQLData> Data(Iterators& itrs) override;
	bool Test(Iterators& itrs) override;

	bool Bind(Func* func, bool precomute) {
		func_ = func;
		precomute_ = precomute;
	}

	const std::string& function_name() {
		return function_name_;
	}

	int num_params() {
		return static_cast<int>(params_.size());
	}

	bool precomute() {
		return precomute_;
	}
	Params& params() {
		return params_;
	}
	
private:
	Func* func_;
	std::string function_name_;
	Params params_;
	bool precomute_;
};

class RangedDataOracle : public IOracle {
};

class ConstantDataOracle : public IOracle {
public:
	ORACLE_IMPL(ConstantDataOracle)

	ConstantDataOracle(std::shared_ptr<ISQLData> constant)
		: constant_(constant) {}

	std::shared_ptr<ISQLData> Data(Iterators& itrs) override {
		return constant_;
	}

	bool Test(Iterators& itrs) override;

private:
	std::shared_ptr<ISQLData> constant_;
};

class IOracleVisitor {
public:
#define DECLVISIT(o) virtual void Visit(o*) = 0;
	SOLID_ORACLE_LIST(DECLVISIT)
#undef DECLVISIT
};



#if 0
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
#endif