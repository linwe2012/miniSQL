#include "functions-manager.h"
#include <chrono>
#include <ctime>

FunctionsManager::FunctionInfo* FunctionsManager::Get(const std::string& function_name)
{
	auto itr = funcs_.find(function_name);
	if (itr == funcs_.end()) {
		return nullptr;
	}
	return &itr->second;
}

void FunctionsManager::Register(std::string name, std::function<void(FunctionPayLoad&)> funcs, bool precompute, int num_params) {
	if (Get(name) == nullptr) {
		console.warn("duplicate function: ", name);
	}
	funcs_[std::move(name)] = FunctionInfo{ std::move(funcs), precompute, num_params };
}

void CurrentTime(FunctionPayLoad& pay) {
	typedef std::chrono::system_clock Clock;

	SQLTimeStampStruct time;
	std::time_t now_c = Clock::to_time_t(Clock::now());
	struct tm* t = std::localtime(&now_c);

	time.date.year = t->tm_year;
	time.date.month = t->tm_mon;
	time.date.date = t->tm_mday;

	time.time.hour = t->tm_hour;
	time.time.minute = t->tm_min;
	time.time.second = t->tm_sec;
	time.fraction = 0;

	pay.return_vals().push_back(std::make_shared<SQLTimeStamp>(new SQLTimeStamp(time)));
}


//TODO
// '2019/12/31 12:20:21:100'
// '2019-8-31 12:20:21:100'
void ToTimeStamp(FunctionPayLoad& pay) {
	SQLTimeStampStruct time;
	auto ptr = pay.params()[1]->Data(*pay.itrs);
	if (!ptr) {
		return;
	}

	auto str = ptr->AsString();
	if (str == nullptr) {
		console.error("totimestamp: unable to parse none string");
		return;
	}

	std::stringstream ss(str->String());

	int tmp;

	while (!ss.eof())
	{
		ss >> tmp;
		int c = ss.get();

		if (c != '/' && c == '-') {

		}
	}
	
	
}

void FunctionsManager::Initilize() {
	Register(
		"current_timestamp", CurrentTime, true, 0
		);
	Register(
		"to_timestamp", ToTimeStamp, false, 1
	);

}
