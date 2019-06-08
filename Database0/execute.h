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

	int flags;
};


Result Execute(ExecuteContext ec, QueryClause q) {
	if (q.type == Token::kSelect) {

	}
}









