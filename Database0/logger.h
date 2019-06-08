#pragma once
#include <iostream>
#include <sstream>
#include <functional>
#include <exception>

class Console {
public:
	struct LogMethod {
		enum {
			kStream,
			kHandler,
		};
		LogMethod() : os(&std::cerr), method(kStream) {}

		int method;
		std::ostream* os;
		std::function<void(std::string)> handler;
		std::stringstream ss;

		template<typename Arg>
		void Msg(Arg arg) {
			if (method == kStream) {
				*os << arg;
				return;
			}

			if (method == kHandler) {
				ss << arg;
				return;
			}
		}

		template<typename Arg>
		void End(Arg arg) {
			if (method == kStream) {
				*os << arg << std::endl;
				return;
			}

			if (method == kHandler) {
				ss << arg;
				handler(ss.str());
				ss.clear();
				return;
			}
		}
	};
#define DEF_LOGGER(severity)                        \
private:                                            \
	LogMethod severity##_;                          \
public:                                             \
	template<typename Arg, typename... Args>        \
	void severity(Arg arg, Args... args) {          \
		if(Severity::severity >= min_severity_) {   \
			severity##_.Msg(arg);                   \
			severity(args...)                       \
		}                                           \
	}                                               \
                                                    \
	template<typename Arg>                          \
	void severity(Arg arg) {                        \
		if(Severity::severity >= min_severity_) {   \
			severity##_.End(arg);                   \
		}                                           \
		if (Severity::severity >= strict_) {        \
			throw std::invalid_argument(nullptr);   \
		}                                           \
	}


	
#define SEVERITY_LIST(V)\
	V(info)             \
	V(log)              \
	V(warn)             \
	V(error)            \
	V(fatal)            \
	V(internal_error)

	SEVERITY_LIST(DEF_LOGGER)
#define ENUM_SEVERITY(name) name,
	enum struct Severity {
		SEVERITY_LIST(ENUM_SEVERITY)
	};
#undef SEVERITY_LIST
#undef DEF_LOGGER
#undef SEVERITY_LIST
	void serverity(Severity s) {
		min_severity_ = s;
	}

private:
	Severity min_severity_ = Severity::log;
	Severity strict_ = Severity::error;
};

extern Console console;

