#pragma once


#include <assert.h>
#include <memory>
#include <string>
#include <functional>
#include <sstream>

// https://docs.microsoft.com/en-us/sql/odbc/reference/appendixes/c-data-types?view=sql-server-2017


// inheritance hierachy
// ISQLData
//   - SQLNull
//   - ISQLNumber 
///       - SQLInt
//       - SQLBigInt
//       - SQLDouble
///       - SQLFloat
///       - SQLNumeric
//   - SQLString
//   - ISQLTimeOrDate
///       - SQLDate
///       - SQLTime
//       - SQLTimeStamp


struct SQLDateStruct {
	SQLDateStruct() {}
	SQLDateStruct(int i)
		: year(i), month(i), date(i) {}
	int year = 0;
	int month = 0;
	int date = 0;
	bool operator==(const SQLDateStruct& rhs) const {
		return year == rhs.year && month == rhs.month && date == rhs.date;
	}
	bool operator!=(const SQLDateStruct& rhs) const {
		return !operator==(rhs);
	}

	bool operator<(const SQLDateStruct& rhs) const {
		if (year > rhs.year) {
			return false;
		}
		if (year < rhs.year) {
			return true;
		}
		if (month > rhs.month) {
			return false;
		}
		if (month < rhs.month) {
			return true;
		}
		if (date >= rhs.date) {
			return false;
		}
		return true;
	}
	bool operator>(const SQLDateStruct& rhs) const {
		return !operator<(rhs) && !operator==(rhs);
	}
};

struct SQLTimeStruct {
	SQLTimeStruct() {}
	SQLTimeStruct(int i)
		: hour(i), minute(i), second(i) {}
	int hour = 0;
	int minute = 0;
	int second = 0;
	bool operator==(const SQLTimeStruct& rhs) const {
		return hour == rhs.hour && minute == rhs.minute && second == rhs.second;
	}
	bool operator<(const SQLTimeStruct& rhs) const {
		if (hour > rhs.hour) {
			return false;
		}
		if (hour < rhs.hour) {
			return true;
		}
		if (minute > rhs.minute) {
			return false;
		}
		if (minute < rhs.minute) {
			return true;
		}
		if (second >= rhs.second) {
			return false;
		}
		return true;
	}
	bool operator>(const SQLTimeStruct& rhs) const {
		return !operator<(rhs) && !operator==(rhs);
	}
};

struct SQLTimeStampStruct {
	SQLTimeStampStruct() {}
	SQLTimeStampStruct(unsigned short i)
		: date(i) , time(i) {}

	SQLDateStruct date;
	SQLTimeStruct time;
	unsigned int fraction = 0;
	bool operator==(const SQLTimeStampStruct& rhs) const {
		return date == rhs.date && time == rhs.time && fraction == rhs.fraction;
	}
	bool operator<(const SQLTimeStampStruct& rhs) const {
		if (date > rhs.date) {
			return false;
		}
		if (date < rhs.date) {
			return true;
		}
		if (time > rhs.time) {
			return false;
		}
		if (time < rhs.time) {
			return true;
		}
		if (fraction >= rhs.fraction) {
			return false;
		}
		return true;
	}
	
};

struct SQLNumericStruct {
	enum { kMaxNumericLen = 16 };
	unsigned char precision;
	char scale;
	unsigned char sign; // The sign field is 1 if positive, 0 if negative.
	unsigned char val[kMaxNumericLen];
};


#define MAKE_SURE(cond, msg) assert(cond)

#define SQL_DATA_TYPE_LIST(V) \
	/*V(Int, int)              */ \
	V(BigInt, int64_t)        \
	V(Double, double)         \
	/*V(Float, float)          */ \
	V(String, std::string)    \
	/*V(Date, SQLDateStruct)    \
	V(Time, SQLTimeStruct)    */\
	V(TimeStamp, SQLTimeStampStruct) \
	/*V(Numeric, SQLNumericStruct) */\
	V(Null, void)

#define FWD_DECALRE(t, u) class SQL##t;
SQL_DATA_TYPE_LIST(FWD_DECALRE)
#undef FWD_DECALRE


class ISQLDataVisitor {
public:
#define VISITOR(t, u)  virtual void Visit(SQL##t *) = 0;
	SQL_DATA_TYPE_LIST(VISITOR)
#undef VISITOR
};

class IConstSQLDataVisitor {
public:
#define VISITOR(t, u)  virtual void Visit(const SQL##t *) = 0;
	SQL_DATA_TYPE_LIST(VISITOR)
#undef VISITOR
};

class ISQLData {
public:
	virtual bool IsNumber()  const { return false; }
	virtual bool IsInt()     const { return false; }
	virtual bool IsBigInt()  const { return false; }
	virtual bool IsFloat()   const { return false; }
	virtual bool IsDouble()  const { return false; }
	virtual bool IsNumeric() const { return false; }

	virtual bool IsString()  const { return false; }
	virtual bool IsBlob()    const { return false; }

	virtual bool IsTimeOrDate() const { return false; }
	virtual bool IsDate()       const { return false; }
	virtual bool IsTime()       const { return false; }
	virtual bool IsTimeStamp()  const { return false; }
	virtual bool IsNull()       const { return false; }

	virtual bool IsVariadic()   const { return false; }

	virtual ~ISQLData() = 0;

	struct RawData {
		const void* ptr = nullptr;
		size_t size = 0;
	};

	virtual RawData GetRaw() const = 0;

#define AS(t, u) virtual SQL##t * As##t() { return nullptr; }  virtual const SQL##t * As##t() const { return nullptr; } 
	SQL_DATA_TYPE_LIST(AS);
#undef AS
	enum CompareResult {
		kLess = -1,
		kLarger = 1,
		kEqual = 0,
		kUnkown = -3,
		kFail = -2 /**< unable to compare */
	};
	virtual CompareResult Compare(const ISQLData* rhs) const = 0;
	virtual void Accept(ISQLDataVisitor*) = 0;
	virtual void Accept(IConstSQLDataVisitor*) const = 0;
	virtual bool Bool() const = 0;

	virtual std::shared_ptr<ISQLData> Add(const ISQLData* r) const = 0;
	virtual std::shared_ptr<ISQLData> Sub(const ISQLData* r) const = 0;
	virtual std::shared_ptr<ISQLData> Mul(const ISQLData* r) const = 0;
	virtual std::shared_ptr<ISQLData> Div(const ISQLData* r) const = 0;
	
};

inline ISQLData::~ISQLData() {}



class SQLNull : public ISQLData {
public:
	SQLNull() {}
	bool IsNull() const override { return true; }
	SQLNull* AsNull() override { return this; }
	const SQLNull* AsNull() const override { return this; }
	void Accept(ISQLDataVisitor*) override;
	void Accept(IConstSQLDataVisitor*) const override;
	CompareResult Compare([[maybe_unused]]const ISQLData* rhs) const override { return kFail; }
	~SQLNull() override {}
	RawData GetRaw() const override { return RawData{ nullptr, 0 }; }
	bool Bool() const override { return false; }
	std::shared_ptr<ISQLData> Add([[maybe_unused]]const ISQLData* r) const override { return std::make_shared<SQLNull>(); }
	std::shared_ptr<ISQLData> Sub([[maybe_unused]]const ISQLData* r) const override { return std::make_shared<SQLNull>(); }
	std::shared_ptr<ISQLData> Mul([[maybe_unused]]const ISQLData* r) const override { return std::make_shared<SQLNull>(); }
	std::shared_ptr<ISQLData> Div([[maybe_unused]]const ISQLData* r) const override { return std::make_shared<SQLNull>(); }
};

class ISQLNumber : public ISQLData {
public:
	bool IsNumber() const override { return true; }
};



#define SOLID_DATA(type, data_type)                 \
bool Is##type () const override { return true; }    \
data_type Value() const { return val_; }            \
void Accept(ISQLDataVisitor*) override;             \
void Accept(IConstSQLDataVisitor*) const override;  \
SQL##type* As##type() override { return this; }     \
const SQL##type* As##type() const override { return this; }     \
SQL##type(data_type val) : val_(val) {}             \
~SQL##type() override {}                            \
RawData GetRaw() const override { return RawData{&val_, sizeof(data_type)};} \
std::shared_ptr<ISQLData> Add(const ISQLData* r) const override;             \
std::shared_ptr<ISQLData> Sub(const ISQLData* r) const override;             \
std::shared_ptr<ISQLData> Mul(const ISQLData* r) const override;             \
std::shared_ptr<ISQLData> Div(const ISQLData* r) const override;             \
bool Bool() const override { return val_ == 0;}


#define CMP_HELPER(type)\
if (rhs->Is##type()) { \
auto v = rhs->As##type()->Value(); \
if (v < Value()) { \
	return kLarger; \
} \
if (v == Value()) { \
	return kEqual; \
} \
return kLess;\
}

/*
class SQLInt : public ISQLNumber {
public:
	SOLID_DATA(Int, int)
private:
	int val_;
}; 
*/

class SQLBigInt : public ISQLNumber {
public:
	using CType = int64_t;
	SOLID_DATA(BigInt, int64_t)
	
	CompareResult Compare(const ISQLData* rhs) const override;

private:
	int64_t val_;
};

/*
class SQLFloat : public ISQLNumber {
public:
	SOLID_DATA(Float, float)
private:
	float val_;
};
*/

class SQLDouble : public ISQLNumber {
public:
	using CType = double;
	SOLID_DATA(Double, double)
	CompareResult Compare(const ISQLData* rhs) const override {
		CMP_HELPER(Double);
		CMP_HELPER(BigInt);
		return kFail;
	}

	/*
	SQLDouble(double val) : val_(val) {}
	bool IsDouble() override { return true; }
	double Value() { return val_; }
	void Accept(ISQLDataVisitor*) override;
	SQLDouble* AsDouble() override { return this; }*/
private:
	double val_;
};

inline ISQLData::CompareResult SQLBigInt::Compare(const ISQLData* rhs) const {
	CMP_HELPER(Double);
	CMP_HELPER(BigInt);
	return kFail;
}

#define DEF_NUMBER_OPS(_type__, _name__, _op__)                                 \
inline std::shared_ptr<ISQLData> _type__::_name__(const ISQLData* r) const {    \
	auto ri = r->AsBigInt();                                                    \
	if (ri != nullptr) {                                                        \
		return std::shared_ptr<ISQLData>(new SQLBigInt(static_cast<int64_t>(Value() _op__ ri->Value())));    \
	}                                                                                  \
	                                                                                   \
	auto rd = r->AsDouble();                                                           \
	if (rd != nullptr) {                                                               \
		return std::shared_ptr<ISQLData>(new SQLDouble(static_cast<double>(Value() _op__ ri->Value())));    \
	}                                                                                  \
	return std::make_shared<SQLNull>();                                                \
}

#define OP_TYPE_LIST(V)\
V(SQLBigInt, Add, +)\
V(SQLBigInt, Sub, -)\
V(SQLBigInt, Mul, *)\
V(SQLBigInt, Div, /)\
V(SQLDouble, Add, +)\
V(SQLDouble, Sub, -)\
V(SQLDouble, Mul, *)\
V(SQLDouble, Div, /)\

OP_TYPE_LIST(DEF_NUMBER_OPS)
#undef OP_TYPE_LIST
#undef DEF_NUMBER_OPS

/*
class SQLNumeric : public ISQLNumber {
public:
	struct Num {
		int64_t integer;
		int64_t decimal;
		int scale;
	};
	SQLNumeric(const char *val, int cnt) : val_(val, cnt) {}
	SQLNumeric(const char* val) : val_(val) {}
	bool IsNumeric() override { return true; }
	const char* Raw() { return val_.c_str(); }
	void Accept(ISQLDataVisitor*) override;
	SQLNumeric* AsNumeric() override { return this; }
	
	Num Value() {
		Num num{ 0, 0 , 0};
		int toggle = 0;
		char dump = '\0';
		std::stringstream ss(val_);
		ss >> num.integer;
		ss >> dump; // eat '.'
		int64_t pos = ss.tellg();
		if (pos < 0) return num;

		ss >> num.decimal;
		num.scale = val_.size() - pos;
		return num;
	}

	static std::string Stringnize(const Num& n) {
		std::stringstream ss;
		ss << n.integer;
		if (n.decimal == 0) return ss.str();
		ss << '.';
		ss << n.decimal;
		return ss.str();
	}
	~SQLNumeric() override {}
	// SOLID_DATA(Numeric, SQLNumericStruct)
private:
	std::string val_;
};
*/

class SQLString : public ISQLData{
public:
	SQLString(const std::string& str) : val_(str) {}
	SQLString(std::string&& str) : val_(std::forward<std::string>(str)) {}
	SQLString(const char * val, size_t n) : val_(val, n) {}
	SQLString(const char *val) : val_(val) {}
	bool IsString() const override { return true; }
	const char* Value()const  { return val_.c_str(); }
	void Accept(ISQLDataVisitor*) override;
	void Accept(IConstSQLDataVisitor*) const override;

	const std::string& String() const { return val_; }

	CompareResult Compare(const ISQLData* rhs) const override {
		auto rs = rhs->AsString();
		if (!rs) {
			return kFail;
		}
		if (rs->val_ == val_) {
			return kEqual;
		}
		if (rs->val_ > val_) {
			return kLess;
		}
		return kLarger;
	}

	SQLString* AsString() override { return this; }
	const SQLString* AsString() const override  { return this; }

	~SQLString() override {}

	RawData GetRaw() const override {
		return RawData{ val_.c_str(), val_.length() + 1 };
	}

	bool IsVariadic() const override { return true; }

	std::shared_ptr<ISQLData> Add(const ISQLData* r) const override {
		auto rs = r->AsString();
		if (rs != nullptr) {
			return std::make_shared<SQLString >(String() + rs->String());
		}
		return std::make_shared<SQLNull>();
	}
	std::shared_ptr<ISQLData> Sub([[maybe_unused]]const ISQLData* r) const override { return std::make_shared<SQLNull>(); }
	std::shared_ptr<ISQLData> Mul([[maybe_unused]]const ISQLData* r) const override { return std::make_shared<SQLNull>(); }
	std::shared_ptr<ISQLData> Div([[maybe_unused]]const ISQLData* r) const override { return std::make_shared<SQLNull>(); }

	bool Bool() const override { return val_.size(); }

private:
	std::string val_;
};


class ISQLTimeOrDate : public ISQLData {
public:
	bool IsTimeOrDate() const override { return true; }
};

/*
class SQLDate : public ISQLTimeOrDate {
public:
	SOLID_DATA(Date, SQLDateStruct)
private:
	SQLDateStruct val_;
};

class SQLTime : public ISQLTimeOrDate {
public:
	SOLID_DATA(Time, SQLTimeStruct)
private:
	SQLTimeStruct val_;
};
*/

class SQLTimeStamp : public ISQLTimeOrDate {
public:
	using CType = SQLTimeStampStruct;
	SOLID_DATA(TimeStamp, SQLTimeStampStruct)
	
	CompareResult Compare(const ISQLData* rhs) const override {
		CMP_HELPER(TimeStamp);
		return kFail;
	}
private:
	SQLTimeStampStruct val_;
};

//TODO(L) implement them
inline std::shared_ptr<ISQLData> SQLTimeStamp::Add([[maybe_unused]]const ISQLData* r) const { return std::make_shared<SQLNull>(); };
inline std::shared_ptr<ISQLData> SQLTimeStamp::Sub([[maybe_unused]]const ISQLData* r) const { return std::make_shared<SQLNull>(); };
inline std::shared_ptr<ISQLData> SQLTimeStamp::Mul([[maybe_unused]]const ISQLData* r) const { return std::make_shared<SQLNull>(); };
inline std::shared_ptr<ISQLData> SQLTimeStamp::Div([[maybe_unused]]const ISQLData* r) const { return std::make_shared<SQLNull>(); };

template <typename T>
struct SQLTypeID {
	constexpr static int value = -1;
	constexpr static const char* name = "invalid_type";
};


template<>
struct SQLTypeID<SQLNull> {
	constexpr static int value = 0;
	constexpr static const char* name = "sqlnull";
};

//template<>
//struct SQLTypeID<SQLInt> {
//	constexpr static int value = 1;
//	constexpr static char* name = "int";
//};

template<>
struct SQLTypeID<SQLBigInt> {
	constexpr static int value = 2;
	constexpr static const char* name = "bigint";
};

template<>
struct SQLTypeID<SQLDouble> {
	constexpr static int value = 3;
	constexpr static const char* name = "double";
};

//template<>
//struct SQLTypeID<SQLFloat> {
//	constexpr static int value = 4;
//	constexpr static char* name = "float";
//};
//
//template<>
//struct SQLTypeID<SQLNumeric> {
//	constexpr static int value = 5;
//	constexpr static char* name = "numeric";
//};

template<>
struct SQLTypeID<SQLString> {
	constexpr static int value = 6;
	constexpr static const char* name = "string";
};

//template<>
//struct SQLTypeID<SQLDate> {
//	constexpr static int value = 7;
//	constexpr static char* name = "date";
//};
//
//template<>
//struct SQLTypeID<SQLTime> {
//	constexpr static int value = 8;
//	constexpr static char* name = "time";
//};

template<>
struct SQLTypeID<SQLTimeStamp> {
	constexpr static int value = 9;
	constexpr static const char* name = "timestamp";
};


#define ACCEPT(t, u) inline void SQL##t::Accept(ISQLDataVisitor* visitor) { visitor->Visit(this); } 
SQL_DATA_TYPE_LIST(ACCEPT)
#undef ACCEPT

#define ACCEPT(t, u) inline void SQL##t::Accept(IConstSQLDataVisitor* visitor) const { visitor->Visit(this); } 
SQL_DATA_TYPE_LIST(ACCEPT)
#undef ACCEPT