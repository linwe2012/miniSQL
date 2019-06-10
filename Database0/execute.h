#pragma once

#include "parser.h"
#include "buffer-manager.h"
#include "index-manager.h"
#include "catalog-manager.h"
#include "record-manager.h"
#include "functions-manager.h"

#include "logger.h"


struct ExecuteContext {
	enum Flag {
		kfOverrideFunctionPrecomputation,
	};

	std::string db;
	BufferManager* bm;
	IndexManager* index;
	CatalogManager* cat;
	FunctionsManager* fun;
	FileId tmp_file;
	PageId tmp_first_page;
	int flags;

	// private:
	bool flag_get_result_;
};


std::shared_ptr<Result> Execute(ExecuteContext& ec, QueryClause& q);









