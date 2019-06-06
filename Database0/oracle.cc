#include "oracle.h"

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
	case Operator::kAnd:  return lhs_->Test(itrs) && rhs_->Test(itrs);
	case Operator::kOr:   return lhs_->Test(itrs) || rhs_->Test(itrs);
	case Operator::kNot:  return !lhs_->Test(itrs);

	case Operator::kEqual:
		if (lmbdFetchData()) {
			return true;
		}
		return l->Compare(r.operator->()) == ISQLData::kEqual;

	case Operator::kAdd:
		break;
	case Operator::kSub:
		break;
	case Operator::kMul:
		break;
	case Operator::kDiv:
		break;
	case Operator::kLess:
		if (lmbdFetchData()) {
			return true;
		}
		return l->Compare(r.operator->()) == ISQLData::kLess;
	case Operator::kGreater:
		if (lmbdFetchData()) {
			return true;
		}
		return l->Compare(r.operator->()) == ISQLData::kLarger;
	case Operator::kLessEq:
		if (lmbdFetchData()) {
			return true;
		}
		cmp = l->Compare(r.operator->());
		return cmp == ISQLData::kLess || cmp == ISQLData::kEqual;
	case Operator::kGreaterEq:
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
	case Operator::kAnd:
		break;
	case Operator::kOr:
		break;
	case Operator::kNot:
		break;
	case Operator::kEqual:
		break;


	case Operator::kAdd:
		if (!lmbdFetchData()) {
			break;
		}
		return l->Add(r.operator->());
	case Operator::kSub:
		if (!lmbdFetchData()) {
			break;
		}
		return l->Sub(r.operator->());
	case Operator::kMul:
		if (!lmbdFetchData()) {
			break;
		}
		return l->Mul(r.operator->());
	case Operator::kDiv:
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
