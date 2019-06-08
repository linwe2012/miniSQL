#pragma once
#include "parser.h"
#include "buffer-manager.h"
#include "index-manager.h"
#include "catalog-manager.h"
#include "record-manager.h"


#include "logger.h"

class FunctionsManager {
public:
	struct FunctionInfo {
		std::function<void(FunctionPayLoad&)> func;
		bool precompute;
		int num_params;
	};

	FunctionInfo* Get(const std::string& function_name);

	void Register(std::string name, std::function<void(FunctionPayLoad&)> funcs, bool precompute, int num_params);

	void Initilize();

private:
	std::map<std::string, FunctionInfo> funcs_;
};