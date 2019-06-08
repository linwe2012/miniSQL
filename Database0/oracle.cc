#include "oracle.h"

#define DECL_ACCPT(o) void o::Accept(IOracleVisitor* visitor) { visitor->Visit(this); }

bool BinopOracle::Test(Iterators& itrs) {
	std::shared_ptr<ISQLData> r;
	std::shared_ptr<ISQLData> l;
	ISQLData::CompareResult cmp;
	auto lmbdFetchData = [this, &itrs, &l, &r] {
		l = lhs_->Data(itrs);
		r = rhs_->Data(itrs);
		if (l == nullptr && r == nullptr) {
			return true;
		}
		return false;
	};

	switch (op_)
	{
	case Token::kAnd:  return lhs_->Test(itrs) && rhs_->Test(itrs);
	case Token::kOr:   return lhs_->Test(itrs) || rhs_->Test(itrs);
	case Token::kNot:  return !lhs_->Test(itrs);

	case Token::kEqual:
		if (lmbdFetchData()) {
			return true;
		}
		return l->Compare(r.operator->()) == ISQLData::kEqual;

	case Token::kAdd:
		break;
	case Token::kSub:
		break;
	case Token::kMul:
		break;
	case Token::kDiv:
		break;
	case Token::kLess:
		if (lmbdFetchData()) {
			return true;
		}
		return l->Compare(r.operator->()) == ISQLData::kLess;
	case Token::kGreater:
		if (lmbdFetchData()) {
			return true;
		}
		return l->Compare(r.operator->()) == ISQLData::kLarger;
	case Token::kLessEq:
		if (lmbdFetchData()) {
			return true;
		}
		cmp = l->Compare(r.operator->());
		return cmp == ISQLData::kLess || cmp == ISQLData::kEqual;
	case Token::kGreaterEq:
		if (lmbdFetchData()) {
			return true;
		}
		return cmp == ISQLData::kLarger || cmp == ISQLData::kEqual;
	default:
		break;
	}
}

std::shared_ptr<ISQLData> BinopOracle::Data(Iterators& itrs) {
	auto l = lhs_->Data(itrs);
	std::shared_ptr<ISQLData> r;
	auto lmbdFetchData = [this, &itrs, &l, &r]() -> bool {
		if (l == nullptr) {
			return false;
		}
		r = rhs_->Data(itrs);
		if (r == nullptr) {
			return false;
		}
		return true;
	};

	switch (op_)
	{
		//TODO(L): Should I throw exception?
	case Token::kAnd:
		break;
	case Token::kOr:
		break;
	case Token::kNot:
		break;
	case Token::kEqual:
		break;


	case Token::kAdd:
		if (!lmbdFetchData()) {
			break;
		}
		return l->Add(r.operator->());
	case Token::kSub:
		if (!lmbdFetchData()) {
			break;
		}
		return l->Sub(r.operator->());
	case Token::kMul:
		if (!lmbdFetchData()) {
			break;
		}
		return l->Mul(r.operator->());
	case Token::kDiv:
		if (!lmbdFetchData()) {
			break;
		}
		return l->Div(r.operator->());
	default:
		break;
	}

	return std::make_shared<ISQLData>(nullptr);
}

std::shared_ptr<ISQLData> VariableOracle::Data(Iterators& itrs)
{
	using Ptr = std::shared_ptr<ISQLData>;
	auto& itr = Itr(itrs);

	if (!Test(itrs)) {
		return Ptr(new SQLNull());
	}

	switch (type_)
	{
	case SQLTypeID<SQLBigInt>::value:
		return Ptr(new SQLBigInt(itr.As<SQLBigInt::CType>()));
	case SQLTypeID<SQLDouble>::value:
		return Ptr(new SQLBigInt(itr.As<SQLDouble::CType>()));
	case SQLTypeID<SQLTimeStamp>::value:
		return Ptr(new SQLTimeStamp(itr.As<SQLTimeStamp::CType>()));
	case SQLTypeID<SQLString>::value:
		return Ptr(new SQLString(
			std::move(itr.Cast<char*>().As<std::string>())
		));
	default:
		// should never come to here!!
		assert(false);
		break;
	}
}

std::shared_ptr<ISQLData> FunctionOracle::Data(Iterators& itrs)
{
	FunctionPayLoad pay{
		&itrs,
		&params_
	};

	(*func_)(pay);
	
	if (pay.return_vals().size() == 0) {
		return std::make_shared<ISQLData>(nullptr);
	}

	return pay.return_vals()[0];
}

bool TestSQLData(std::shared_ptr<ISQLData> data) {
	if (!data) {
		return false;
	}

	return data->Bool();
}

bool FunctionOracle::Test(Iterators& itrs)
{
	return TestSQLData(Data(itrs));
}

bool ConstantDataOracle::Test(Iterators& itrs)
{
	return TestSQLData(Data(itrs));
}
