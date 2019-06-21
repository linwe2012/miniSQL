#include "execute.h"
#include "bptree.h"
#include "cluster-bptree.h"
#define RELEASE

void DeduceColumnInfo(ExecuteContext& e, QueryClause& q);

void PluginFunctions(ExecuteContext& e, QueryClause& q);

void OptimizeWhereClause(ExecuteContext& e, QueryClause& q);

void RefineWhereClause(ExecuteContext& e, QueryClause& q) {
	DeduceColumnInfo(e, q);
	PluginFunctions(e, q);
	OptimizeWhereClause(e, q);
}

void ExplodeNamesToColumnInfo(ExecuteContext& e, ColumnName* pcol, const std::string name) {
	ColumnName& col = *pcol;
	std::vector<std::string> names = Explode(name, '.');
	if (names.size() > 3) {
		console.warn("Too many specifiers in column name: ", name);
	}
	col.column_name = names.back(); names.pop_back();

	if (names.size()) {
		if (col.table_name.size()) {
			console.warn("Column is named with '.', which may be misinterpreted");
		}
		col.table_name = names.back(); names.pop_back();
	}

	if (names.size()) {
		col.db_name = names.back(); names.pop_back();
	}
	else {
		col.db_name = e.db;
	}
}


int DeduceColumnTableName(ExecuteContext& e, QueryClause& q, ColumnName* pcol) {
	ColumnName& col = *pcol;
	int id = -1;

	if (col.table_name.size()) {
		auto tb = q.tables.find(UnqiueTable{ col.db_name, col.table_name });
		if (tb == q.tables.end()) {
			console.error("Unresolved column: database: ", col.db_name, "table: ", col.table_name, "is not presented in from clause");
		}
		col.table_name = tb->second->name;
		auto itr = tb->second->column_bindings.find(col.column_name);
		if (itr != tb->second->column_bindings.end()) {
			return itr->second;
		}

		Attribute* attrib = &e.cat->FetchColumn(col);
		id = q.variable_attribs.size();
		q.variable_attribs.push_back(QueryVariable{ attrib });
		tb->second->column_bindings[col.column_name] = id;
		return id;
	}

	std::string table_name;
	std::string alias;

	for (auto& table : q.vectables) {
		auto itr = table.column_bindings.find(col.column_name);
		if (itr != table.column_bindings.end()) {
			id = itr->second;
			table_name = table.name;
			if (table.alias.size()) {
				table_name += " aliased as: " + table.alias;
			}
			continue;
		}

		auto& meta = e.cat->FetchTable(table.db_name, table.name);
		try {
			col.table_name = table.name;
			Attribute* attrib = &e.cat->FetchColumn(col);
			if (id != -1) {
				std::string tb = table_name;
				if (alias.size()) {
					tb += " aliased as: " + table.alias;
				}
				console.error("Unable to resolve ambiguous table: column ",
					col.column_name, " has 2 possible table: ", col.table_name, " or ", tb);
			}


			id = q.variable_attribs.size();
			q.variable_attribs.push_back(QueryVariable{ attrib });
			table.column_bindings[col.column_name] = id;
			table_name = table.name;
			alias = table.alias;
		}
		catch (...) {
			// not found
		}
	}

	col.table_name = table_name;
	return id;
}

void DeduceColumnInfo(ExecuteContext& e, QueryClause& q) {
	// check if table exists & if they are dubbed causing naming collision
	for (auto& table : q.vectables) {
		if (table.db_name.size() == 0) {
			table.db_name = e.db;
		}

		q.tables[UnqiueTable{ table.db_name, table.name, false }] = &table;

		{
			auto itr = q.tables.find(UnqiueTable{
				table.db_name,
				table.alias
				}
			);

			if (itr != q.tables.end()) {
				console.warn("Table alias collide with previous table name or alias: ",
					itr->first.db, ".", itr->first.name);
			}

			e.cat->FetchTable(table.db_name, table.name);
			if (table.alias.size()) {
				q.tables[UnqiueTable{ table.db_name, table.alias, true }] = &table;
			}
		};
	}

	// deduce variables i.e. WHERE database.table.columnname or (just) columname
	for (auto var : q.variables) {
		ColumnName col; // "dabase.table.columnname"
		ExplodeNamesToColumnInfo(e, &col, var->name());

		int id = DeduceColumnTableName(e, q, &col);
		if (id < 0) {
			console.error("Unresolved table: database: ", col.db_name, 
				" column: ", col.column_name, ". Please check table is presented in from clause");
		}

		var->Bind(static_cast<int>(id), q.variable_attribs[id].attrib->type);
	}

	if (q.select.size() == 0) {
		console.error("Nothing is selected");
	}

	// handle SELECT *
	for (auto col : q.select) {
		if (col.column_name == "*") {
			if (q.select.size() > 1) {
				console.warn("Exra columns when asterisk present, which will be obscured");
			}
			q.select.clear();
			for (auto table : q.vectables) {
				auto& meta = e.cat->FetchTable(table.db_name, table.name);
				for (auto attrib : meta.attributes) {
					q.select.push_back(ColumnName{
						meta.db_name,
						meta.table_name,
						attrib.column_name
						});
				}
			}
		}
	}

	// deduce SELECT column
	for (auto col : q.select) {
		ExplodeNamesToColumnInfo(e, &col, col.column_name);
		int id = DeduceColumnTableName(e, q, &col);
		q.select_result.push_back(id);
	}

	// check if table decalred in FROM but never used
	for (auto table : q.vectables) {
		if (table.column_bindings.size() == 0) {
			std::string alias;
			if (table.alias.size()) {
				alias = "aliased as " + table.alias;
			}
			console.warn("Unreferenced table: ", "(database):", table.db_name,
				" (table):", table.name, alias, " is decalared in from clause but is never used");
		}
	}
}

void PluginFunctions(ExecuteContext& e, QueryClause& q)
{
	for (auto f : q.functions) {
		auto ptr = e.fun->Get(f->function_name());
		if (ptr == nullptr) {
			console.fatal("Undecalred function: ", f->function_name());
		}

		if (ptr->num_params >= 0) {
			if (ptr->num_params != f->num_params()) {
				console.fatal("Function ", f->function_name(), "requires ",
					f->num_params(), " paramters", ", yet", ptr->num_params, "is provided");
			}
		}

		f->Bind(&ptr->func, ptr->precompute);
	}
}


class Optimizer : public IOracleVisitor {
public:
	Optimizer(ExecuteContext& _e, QueryClause& _q)
		:e(_e), q(_q) {}

	struct TraceInfo {
		IOracle* trace;
		int level;
	};
#define DO_NOTHING(...)

#define DEF_CNT(Name, literal, prec) int Name = 0;
#define CASE_INC(Name, ...) case Token::k##Name: ++Name; break;
#define CASE_DEC(Name, ...) case Token::k##Name: --Name; break;

	struct BinopCount {
		OPERATOR_LIST(DEF_CNT, DO_NOTHING)

		void Inc(BinopOracle* c) {
			switch (c->op())
			{
				OPERATOR_LIST(CASE_INC, DO_NOTHING)
			default:
				break;
			}
		}

		void Dec(BinopOracle* c) {
			switch (c->op())
			{
				OPERATOR_LIST(CASE_DEC, DO_NOTHING)
			default:
				break;
			}
		}
	};
	
#undef DEF_CNT
#undef DO_NOTHING
#undef CASE_INC
#undef CASE_DEC

	struct Tracer {
		Tracer(Optimizer* _self, IOracle* _o, int param = 0)
			:self(_self), o(_o)
		{
			auto& trace = _self->trace_;
			int level = (trace.size() == 0) ? 0 : (trace.back().level + 1);

			_self->trace_.push_back(TraceInfo{
					_o,
					level
				});
		}

		~Tracer() {
			if (self->trace_.back().trace != o) {
				console.internal_error("Optimizer: stack corrupted");
			}
			
			self->trace_.pop_back();
		}

		Optimizer* self;
		IOracle* o;
	};

	struct BinopHelper {
		bool flag_not = false;
	};

	struct BinopTracer {
		BinopTracer(Optimizer* _self, BinopOracle* _o) {
			local.flag_not = _self->binop_trace_.flag_not;
			if (_o->op() == Token::kNot) {

			}
		}

		BinopHelper local;
	};

	
	void Visit(BinopOracle* c) override {
		Tracer tracer(this, c);
		bin_cnt_.Inc(c);

		c->lhs()->Accept(this);
		if (return_val_) {
			c->set_lhs(return_val_);
			return_val_.reset();
		}

		c->rhs()->Accept(this);
		if (return_val_) {
			c->set_rhs(return_val_);
			return_val_.reset();
		}

		bin_cnt_.Dec(c);
		IsIndexable(c);
	}

	void Visit(FunctionOracle* c) override {
		for (auto& p : c->params()) {
			p->Accept(this);
			if (return_val_) {
				p = return_val_;
				return_val_.reset();
			}
		}

		if (c->precomute()) {
			IOracle::Iterators itrs;
			return_val_.reset(new ConstantDataOracle(c->Data(itrs)));
		}
	}

	

	void IsIndexable(BinopOracle* c) {
		//TODO: This is still optimizatizable if nots are cancelled out
		if (bin_cnt_.Not + bin_cnt_.Or + bin_cnt_.NotEqual) {
			return;
		}

		if (!c->lhs() || !c->rhs()) {
			return;
		}

		auto constant = c->lhs()->AsConstantDataOracle();
		auto var = c->rhs()->AsVariableOracle();
		if (!constant || !var) {
			constant = c->rhs()->AsConstantDataOracle();
			var = c->lhs()->AsVariableOracle();
		}
		if (!constant || !var) {
			return;
		}

		auto& qvar = q.variable_attribs[var->id()];
		if (qvar.attrib->first_index.IsNil()) {
			return;
		}

		if (c->op() == Token::kEqual) {
			IOracle::Iterators itrs;
			qvar.index_target = constant->Data(itrs);
			qvar.index_method = QueryVariable::kDirectIndex;
			return;
		}

		if (Token::IsCompare(c->op())) {
			if(qvar.attrib->is_primary_key) {
				IOracle::Iterators itrs;
				qvar.index_method = QueryVariable::kPrimaryRangeIndex;
				qvar.index_target = constant->Data(itrs);
			}
		}
	}

	//TODO
	void Visit(VariableOracle* c) override {}
	
	//TODO: Dont think this need optimzation
	void Visit(ConstantDataOracle* c)override  {}

	friend struct Tracer;
	ExecuteContext& e;
	QueryClause& q;

	BinopHelper binop_trace_;
	BinopCount bin_cnt_;
	std::shared_ptr<IOracle> return_val_;
	std::vector<TraceInfo> trace_;
};

// Driver for optimizer
void OptimizeWhereClause(ExecuteContext& e, QueryClause& q) {
	Optimizer optimizer(e, q);
	q.where_oracle->Accept(&optimizer);
}


void FilterRoll(ExecuteContext& e, QueryClause& q, IOracle::Iterators& itrs, int table_id, Result& r);

void Filter(ExecuteContext& e, QueryClause& q, Result& r) {
	IOracle::Iterators itrs;
	for (auto var_attirb : q.variable_attribs) {
		itrs.push_back(e.bm->GetPage<char>(var_attirb.attrib->file, var_attirb.attrib->first_page));
	}
	FilterRoll(e, q, itrs, 0, r);
}

void PipeData(QueryClause& q, IOracle::Iterators& itrs, Result& r) {
	std::vector<std::shared_ptr<ISQLData>> data;
	for (int i : q.select_result) {
		auto& itr = itrs[i];
		if (itr.IsNil()) {
			data.push_back(std::make_shared<SQLNull>());
			continue;
		}
		using Ptr = std::shared_ptr<ISQLData>;
		switch (q.variable_attribs[i].attrib->type)
		{
		case SQLTypeID<SQLBigInt>::value:
			data.push_back(Ptr(new SQLBigInt(itr.As<SQLBigInt::CType>()))); break;
		case SQLTypeID<SQLDouble>::value:
			data.push_back(Ptr(new SQLDouble(itr.As<SQLDouble::CType>()))); break;
		case SQLTypeID<SQLTimeStamp>::value:
			data.push_back(Ptr(new SQLTimeStamp(itr.As<SQLTimeStamp::CType>()))); break;
		case SQLTypeID<SQLString>::value:
			data.push_back(Ptr(new SQLString(
				itr.Cast<VarCharFixed256>().As<VarCharFixed256>().data
			))); break;
		default:
			break;
		}
	}
	r.requested.push_back(std::move(data));
}


void FilterRoll(ExecuteContext& e, QueryClause& q, IOracle::Iterators& itrs, int table_id, Result& r) {
	if (table_id >= q.vectables.size()) {
		return;
	}

	auto& table = q.vectables[table_id];
	if (table.column_bindings.size() == 0) {
		return;
	}
	bool flag_oneshot = false;
	int64_t offset = -1;
	for (auto& id_itr : table.column_bindings) {

		int id = id_itr.second;
		auto attrib = q.variable_attribs[id].attrib;
		if (q.variable_attribs[id].index_method == QueryVariable::kDirectIndex) {
			auto& meta = e.cat->FetchTable(table.db_name, table.name);
			auto& prim = meta.attributes[meta.primary_keys[0]];
			switch (q.variable_attribs[id].attrib->type)
			{
			case SQLTypeID<SQLBigInt>::value:
			case SQLTypeID<SQLDouble>::value:

			case SQLTypeID<SQLTimeStamp>::value:
				break;
			case SQLTypeID<SQLString>::value:
				switch (prim.type)
				{
				case SQLTypeID<SQLBigInt>::value:
				{
					BPTree<VarCharFixed256, SQLBigInt::CType> bp(e.bm, attrib->file, attrib->first_index);
					VarCharFixed256 fixed;
					memcpy(fixed.data, q.variable_attribs[id].index_target->AsString()->String().c_str(),
						q.variable_attribs[id].index_target->AsString()->String().size() + 1);
					auto pair = bp.Lookup(fixed);
					ClusteredBPTree<SQLBigInt::CType> cbp(e.bm, prim.file, prim.first_index);
					offset = cbp.ComputeOffset((*pair).prim, prim.first_page);
					flag_oneshot = true;
				}
				break;
				case SQLTypeID<SQLDouble>::value:

				case SQLTypeID<SQLTimeStamp>::value:
					break;
				case SQLTypeID<SQLString>::value:
					break;
				default:
					break;
				}
				break;
			default:
				break;
			}
		}
		if (flag_oneshot) {
			break;
		}
	}

	// rewind pointers to be beginning
	for (auto& id_itr : table.column_bindings) {

		int id = id_itr.second;
		auto attrib = q.variable_attribs[id].attrib;

		itrs[id] = e.bm->GetPage<char>(attrib->file, attrib->first_page);
		switch (attrib->type)
		{
		case SQLTypeID<SQLBigInt>::value:
		case SQLTypeID<SQLDouble>::value:
			itrs[id].SetStep(sizeof(double));
			break;
		case SQLTypeID<SQLTimeStamp>::value:
			itrs[id].SetStep(sizeof(SQLTimeStampStruct));
			break;
		case SQLTypeID<SQLString>::value:
			itrs[id].SetStep(sizeof(VarCharFixed256));
			break;
		default:
			break;
		}
		if (flag_oneshot) {
			itrs[id] += offset;
			while (itrs[id].IsDeleted())
			{
				++itrs[id];
				if (itrs[id].IsEnd()) {
					break;
				}
			}
		}
	}

	int first = table.column_bindings.begin()->second;

	while (!itrs[first].IsEnd()) {
		FilterRoll(e, q, itrs, table_id + 1, r);
		if (table_id == q.vectables.size() - 1) {
			if (q.where_oracle->Test(itrs)) {
				if (q.type != Token::kDelete) {
					table.rows.push_back(itrs[first].row());
					PipeData(q, itrs, r);
				}
				if (q.type == Token::kDelete) {
					for (auto& itr : itrs) {
						itr.MarkDeleted();
					}
				}
			}
		}
		

		for (auto& id_itr : table.column_bindings) {
			int id = id_itr.second;
			//if (q.variable_attribs[id].attrib->type == SQLTypeID<SQLString>::value) {
			//	auto tmp = (itrs[id].Cast<char*>() + 1);
			//	if (tmp.IsEndPage()) {
			//		++tmp;
			//	}
			//	itrs[id] = tmp.Cast<char>();
			//}
			//else
			//{
				++itrs[id];
				while (itrs[id].IsEndPage() || itrs[id].IsDeleted())
				{
					++itrs[id];
					if (itrs[id].IsEnd()) {
						break;
					}
				}
				// if (itrs[id].IsEndPage()) {
				// 	++itrs[id];
				// }
			//}
			
			
		}

		if (itrs[first].IsEnd() || flag_oneshot) {
			break;
		}

		
		
	}
}





void ExecuteSelect(ExecuteContext& e, QueryClause& q, Result& r) {
	RefineWhereClause(e, q);
	Filter(e, q, r);
	for (int i : q.select_result) {
		r.attribs.push_back(q.variable_attribs[i].attrib);
	}
	r.num_rows_affected = 0;
}


void ExecuteInsert(ExecuteContext& e, QueryClause& q, Result& r) {
	auto vec = Explode(q.insert_table, '.');
	std::string table = vec.back(); vec.pop_back();
	std::string db;

	if (vec.size()) {
		db = vec.back();
	}
	else {
		db = e.db;
	}
	auto& meta = e.cat->FetchTable(db, table);

	if (q.insert_col.size() == 0) {
		for (auto& attrib : meta.attributes) {
			q.insert_col.push_back(
				ColumnName{
					db,
					table,
					attrib.column_name
				});
		}
	}

	int sz = std::min(q.insert_col.size(), q.insert_data.size());
	if (q.insert_col.size() != q.insert_data.size()) {
		console.error("Insert number of columns mismatch data provided");
	}
	
	
	
	int pid = meta.primary_keys[0];
	auto& prim = meta.attributes[pid];
	ClusteredBPTree<char> bp(e.bm, prim.file, prim.first_index, prim.first_page);

	std::vector<std::shared_ptr<ISQLData>> binding;
	binding.resize(meta.attributes.size());

	for (int i = 0; i < sz; ++i) {
		auto itr = meta.attributes_map.find(q.insert_col[i].column_name);
		if (itr == meta.attributes_map.end()) {
			console.error("Column: ", q.insert_col[i].column_name,
				" is not memeber of table ", meta.table_name
			);
		}
		else {
			size_t offset = std::distance(&(*meta.attributes.begin()), itr->second);
			binding[offset] = q.insert_data[i];
		}
		
	}

	auto data_prim = binding[pid];
	uint64_t offset = 0;
	
	if (prim.type == SQLTypeID<SQLBigInt>::value) {
		auto abp = bp.Cast<SQLBigInt::CType>();
		if (prim.first_index.IsNil()) {
			prim.first_index = abp.BuildIndex(e.bm->GetPage<SQLBigInt::CType>(prim.file, prim.first_page));
		}
		prim.first_index = abp.Insert(data_prim->AsBigInt()->Value());
		offset = abp.ComputeOffset(data_prim->AsBigInt()->Value(), prim.first_page);
	}
	else if (prim.type == SQLTypeID<SQLDouble>::value) {
		using Type = SQLDouble;
		auto abp = bp.Cast<Type::CType>();
		if (prim.first_index.IsNil()) {
			prim.first_index = abp.BuildIndex(e.bm->GetPage<Type::CType>(prim.file, prim.first_page));
		}
		prim.first_index = abp.Insert(data_prim->AsDouble()->Value());
		offset = abp.ComputeOffset(data_prim->AsDouble()->Value(), prim.first_page);
	}
	else if (prim.type == SQLTypeID<SQLTimeStamp>::value) {
		using Type = SQLTimeStamp;
		auto abp = bp.Cast<Type::CType>();
		if (prim.first_index.IsNil()) {
			prim.first_index = abp.BuildIndex(e.bm->GetPage<Type::CType>(prim.file, prim.first_page));
		}
		prim.first_index = abp.Insert(data_prim->AsTimeStamp()->Value());
		offset = abp.ComputeOffset(data_prim->AsTimeStamp()->Value(), prim.first_page);
	}
	else if (prim.type == SQLTypeID<SQLString>::value) {
		auto abp = bp.Cast<VarCharFixed256>();
		if (prim.first_index.IsNil()) {
			prim.first_index = abp.BuildIndex(e.bm->GetPage<VarCharFixed256>(prim.file, prim.first_page));
		}
		VarCharFixed256 varchar;
		memcpy(varchar.data, data_prim->AsString()->String().c_str(), data_prim->AsString()->String().size() + 1);
		prim.first_index = abp.Insert(varchar);
		// prim.first_index = abp.Insert(data_prim->AsString()->Value());
		offset = abp.ComputeOffset(varchar, prim.first_page);
	}
	
	
	VarCharFixed256 varchar;
	int64_t int_val;
	double dou_val;
	bool is_int;
	auto GetVal = [&int_val, &dou_val, &is_int](std::shared_ptr<ISQLData> d) {
		if (d->AsBigInt()) {
			is_int = true;
			int_val = d->AsBigInt()->Value();
		}
		else if(d->AsDouble()) {
			dou_val = d->AsDouble()->Value();
			is_int = false;
		}
	};
#define INTDOU is_int ? int_val : dou_val
	for (int i = 0; i < binding.size(); ++i) {
		
		auto& attrib = meta.attributes[i];
		if (attrib.is_primary_key) {
			continue;
		}
		BPTree<char, char> bpp(e.bm, attrib.file, attrib.first_index);
		auto itr = e.bm->GetPage<char>(attrib.file, attrib.first_page);
		switch (attrib.type)
		{
		case SQLTypeID<SQLBigInt>::value:
			if (!binding[i]) {
				auto c = (itr.Cast<SQLBigInt::CType>() + offset);
				if ( !c.InsertNil(1) ) {
					c.SplitPage();
					if (!c.InsertNil(1)) {
						++c;
						c.InsertNil(1);
					}
				}
				// (itr.Cast<SQLBigInt::CType>() + offset).InsertNil(1);
			}
			else {
				auto c = (itr.Cast<SQLBigInt::CType>() + offset);
				GetVal(binding[i]);

				if (!c.Insert(INTDOU)) {
					c.SplitPage();
					if (!c.Insert(INTDOU)) {
						++c;
						c.Insert(INTDOU);
					}
					
				}
			}
			if (!attrib.first_index.IsNil()) {
				using T = SQLBigInt::CType;
				switch (prim.type)
				{
				case SQLTypeID<SQLBigInt>::value:
					attrib.first_index = bpp.Cast<T, SQLBigInt::CType>().Insert(INTDOU, data_prim->AsBigInt()->Value());
					break;
				case SQLTypeID<SQLDouble>::value:
					attrib.first_index = bpp.Cast<T, SQLDouble::CType>().Insert(INTDOU, data_prim->AsDouble()->Value());
					break;
				case SQLTypeID<SQLTimeStamp>::value:
					attrib.first_index = bpp.Cast<T, SQLTimeStamp::CType>().Insert(INTDOU, data_prim->AsTimeStamp()->Value());
					break;
				case SQLTypeID<SQLString>::value:
					memcpy(varchar.data, data_prim->AsString()->String().c_str(), data_prim->AsString()->String().size());
					attrib.first_index = bpp.Cast<T, VarCharFixed256>().Insert(INTDOU, varchar);
					break;
				default:
					break;
				}
			}
			break;
		case SQLTypeID<SQLDouble>::value:
			if (!binding[i]) {
				auto c = (itr.Cast<SQLDouble::CType>() + offset);
				if (!c.InsertNil(1)) {
					c.SplitPage();
					c.InsertNil(1);
				}
				// (itr.Cast<SQLBigInt::CType>() + offset).InsertNil(1);
			}
			else {
				auto c = (itr.Cast<SQLDouble::CType>() + offset);
				GetVal(binding[i]);
				if (!c.Insert(INTDOU)) {
					c.SplitPage();
					if (!c.Insert(INTDOU)) {
						++c;
						c.Insert(INTDOU);
					}
				}
			}
			if (!attrib.first_index.IsNil()) {
				using T = SQLDouble::CType;
				switch (prim.type)
				{
				case SQLTypeID<SQLBigInt>::value:
					attrib.first_index = bpp.Cast<T, SQLBigInt::CType>().Insert(INTDOU, data_prim->AsBigInt()->Value());
					break;
				case SQLTypeID<SQLDouble>::value:
					attrib.first_index = bpp.Cast<T, SQLDouble::CType>().Insert(INTDOU, data_prim->AsDouble()->Value());
					break;
				case SQLTypeID<SQLTimeStamp>::value:
					attrib.first_index = bpp.Cast<T, SQLTimeStamp::CType>().Insert(INTDOU, data_prim->AsTimeStamp()->Value());
					break;
				case SQLTypeID<SQLString>::value:
					memcpy(varchar.data, data_prim->AsString()->String().c_str(), data_prim->AsString()->String().size());
					attrib.first_index = bpp.Cast<T, VarCharFixed256>().Insert(INTDOU, varchar);
					break;
				default:
					break;
				}
			}
			break;
		case SQLTypeID<SQLTimeStamp>::value:
			if (!binding[i]) {
				(itr.Cast<SQLTimeStamp::CType>() + offset).InsertNil(1);
			}
			else {
				(itr.Cast<SQLTimeStamp::CType>() + offset).Insert(binding[i]->AsTimeStamp()->Value());
			}
			if (!attrib.first_index.IsNil()) {
				using T = SQLTimeStamp::CType;
				switch (prim.type)
				{
				case SQLTypeID<SQLBigInt>::value:
					attrib.first_index = bpp.Cast<T, SQLBigInt::CType>().Insert(binding[i]->AsBigInt()->Value(), data_prim->AsBigInt()->Value());
					break;
				case SQLTypeID<SQLDouble>::value:
					attrib.first_index = bpp.Cast<T, SQLDouble::CType>().Insert(binding[i]->AsBigInt()->Value(), data_prim->AsDouble()->Value());
					break;
				case SQLTypeID<SQLTimeStamp>::value:
					attrib.first_index = bpp.Cast<T, SQLTimeStamp::CType>().Insert(binding[i]->AsBigInt()->Value(), data_prim->AsTimeStamp()->Value());
					break;
				case SQLTypeID<SQLString>::value:
					memcpy(varchar.data, data_prim->AsString()->String().c_str(), data_prim->AsString()->String().size());
					attrib.first_index = bpp.Cast<T, VarCharFixed256>().Insert(binding[i]->AsBigInt()->Value(), varchar);
					break;
				default:
					break;
				}
			}
			break;
		case SQLTypeID<SQLString>::value:
			if (!binding[i]) {
				auto c = (itr.Cast<VarCharFixed256>() + offset);
				if (!c.InsertNil(1)) {
					c.SplitPage();
					if (!c.InsertNil(1)) {
						++c;
						c.InsertNil(1);
					}
				}
				// (itr.Cast<VarCharFixed256>() + offset).InsertNil(1);
				// (itr.Cast<char*>() + offset).InsertNil(1);
			}
			else {
				VarCharFixed256 varchar;
				memcpy(varchar.data, binding[i]->AsString()->String().c_str(), binding[i]->AsString()->String().size()+1);
				auto c = (itr.Cast<VarCharFixed256>() + offset);
				if (!c.Insert(varchar)) {
					c.SplitPage();

					if (!c.Insert(varchar)) {
						++c;
						c.Insert(varchar);
					}
				}
				// (itr.Cast<VarCharFixed256>() + offset).Insert(varchar);
				// (itr.Cast<char*>() + offset).Insert(binding[i]->AsString()->String());
			}
			if (!attrib.first_index.IsNil()) {
				using T = SQLTimeStamp::CType;
				switch (prim.type)
				{
				case SQLTypeID<SQLBigInt>::value:
					attrib.first_index = bpp.Cast<T, SQLBigInt::CType>().Insert(binding[i]->AsBigInt()->Value(), data_prim->AsBigInt()->Value());
					break;
				case SQLTypeID<SQLDouble>::value:
					attrib.first_index = bpp.Cast<T, SQLDouble::CType>().Insert(binding[i]->AsBigInt()->Value(), data_prim->AsDouble()->Value());
					break;
				case SQLTypeID<SQLTimeStamp>::value:
					attrib.first_index = bpp.Cast<T, SQLTimeStamp::CType>().Insert(binding[i]->AsBigInt()->Value(), data_prim->AsTimeStamp()->Value());
					break;
				case SQLTypeID<SQLString>::value:
					memcpy(varchar.data, data_prim->AsString()->String().c_str(), data_prim->AsString()->String().size());
					attrib.first_index = bpp.Cast<T, VarCharFixed256>().Insert(binding[i]->AsBigInt()->Value(), varchar);
					break;
				default:
					break;
				}
			}
			break;
		default:
			break;
		}
	}
}

void ExecuteCreateTable(ExecuteContext& e, QueryClause& q, Result& r) {
	auto vec = Explode(q.create_table, '.');
	std::string tb = vec.back(); vec.pop_back();
	std::string db;
	if (vec.size()) {
		db = vec.back();
	}

	if (db.size() == 0) {
		db = e.db;
	}

	bool has_primary = false;
	std::string primary;
	for (auto p : q.create) {
		if (p.is_primary_key) {
			has_primary = true;
			primary = p.name.column_name;
		}
	}

	if (!has_primary) {
		console.error("Expected a primary key");
	}
	e.cat->NewTable(db, tb, q.create);
	// auto& table = e.cat->FetchTable(db, tb);
	// auto& col = e.cat->FetchColumn(ColumnName{
	// 	db, tb, primary
	// 	});
}

void ExecuteCreateIndex(ExecuteContext& e, QueryClause& q, Result& r) {
	q.index.db_name = e.db;
	auto& attrib = e.cat->FetchColumn(q.index);
	auto& meta = e.cat->FetchTable(q.index.db_name, q.index.table_name);
	auto& prim = meta.attributes[meta.primary_keys[0]];
	BPTree<char, char> bp(e.bm, attrib.file, attrib.first_index);
	attrib.index_name = q.create_index;
	switch (attrib.type)
	{
	case SQLTypeID<SQLBigInt>::value:
	{
		using T = SQLBigInt;
		switch (prim.type)
		{
		case SQLTypeID<SQLBigInt>::value:
			attrib.first_index = bp.Cast<T::CType, SQLBigInt::CType>().BuildIndex(
				e.bm->GetPage<T::CType>(attrib.file, attrib.first_page),
				e.bm->GetPage<SQLBigInt::CType>(prim.file, prim.first_page)
			);
			break;
		case SQLTypeID<SQLDouble>::value:
			attrib.first_index = bp.Cast<T::CType, SQLDouble::CType>().BuildIndex(
				e.bm->GetPage<T::CType>(attrib.file, attrib.first_page),
				e.bm->GetPage<SQLDouble::CType>(prim.file, prim.first_page)
			);
			break;
		case SQLTypeID<SQLTimeStamp>::value:
			attrib.first_index = bp.Cast<T::CType, SQLTimeStamp::CType>().BuildIndex(
				e.bm->GetPage<T::CType>(attrib.file, attrib.first_page),
				e.bm->GetPage<SQLTimeStamp::CType>(prim.file, prim.first_page)
			);
			break;
		case SQLTypeID<SQLString>::value:
			attrib.first_index = bp.Cast<T::CType, VarCharFixed256>().BuildIndex(
				e.bm->GetPage<T::CType>(attrib.file, attrib.first_page),
				e.bm->GetPage<VarCharFixed256>(prim.file, prim.first_page)
			);
			break;
		default:
			break;
		}
	}
		break;
	case SQLTypeID<SQLDouble>::value:
	{
		using T = SQLDouble;
		switch (prim.type)
		{
		case SQLTypeID<SQLBigInt>::value:
			attrib.first_index = bp.Cast<T::CType, SQLBigInt::CType>().BuildIndex(
				e.bm->GetPage<T::CType>(attrib.file, attrib.first_page),
				e.bm->GetPage<SQLBigInt::CType>(prim.file, prim.first_page)
			);
			break;
		case SQLTypeID<SQLDouble>::value:
			attrib.first_index = bp.Cast<T::CType, SQLDouble::CType>().BuildIndex(
				e.bm->GetPage<T::CType>(attrib.file, attrib.first_page),
				e.bm->GetPage<SQLDouble::CType>(prim.file, prim.first_page)
			);
			break;
		case SQLTypeID<SQLTimeStamp>::value:
			attrib.first_index = bp.Cast<T::CType, SQLTimeStamp::CType>().BuildIndex(
				e.bm->GetPage<T::CType>(attrib.file, attrib.first_page),
				e.bm->GetPage<SQLTimeStamp::CType>(prim.file, prim.first_page)
			);
			break;
		case SQLTypeID<SQLString>::value:
			attrib.first_index = bp.Cast<T::CType, VarCharFixed256>().BuildIndex(
				e.bm->GetPage<T::CType>(attrib.file, attrib.first_page),
				e.bm->GetPage<VarCharFixed256>(prim.file, prim.first_page)
			);
			break;
		default:
			break;
		}
	}
		break;
	case SQLTypeID<SQLTimeStamp>::value:
	{
		using T = SQLTimeStamp;
		switch (prim.type)
		{
		case SQLTypeID<SQLBigInt>::value:
			attrib.first_index = bp.Cast<T::CType, SQLBigInt::CType>().BuildIndex(
				e.bm->GetPage<T::CType>(attrib.file, attrib.first_page),
				e.bm->GetPage<SQLBigInt::CType>(prim.file, prim.first_page)
			);
			break;
		case SQLTypeID<SQLDouble>::value:
			attrib.first_index = bp.Cast<T::CType, SQLDouble::CType>().BuildIndex(
				e.bm->GetPage<T::CType>(attrib.file, attrib.first_page),
				e.bm->GetPage<SQLDouble::CType>(prim.file, prim.first_page)
			);
			break;
		case SQLTypeID<SQLTimeStamp>::value:
			attrib.first_index = bp.Cast<T::CType, SQLTimeStamp::CType>().BuildIndex(
				e.bm->GetPage<T::CType>(attrib.file, attrib.first_page),
				e.bm->GetPage<SQLTimeStamp::CType>(prim.file, prim.first_page)
			);
			break;
		case SQLTypeID<SQLString>::value:
			attrib.first_index = bp.Cast<T::CType, VarCharFixed256>().BuildIndex(
				e.bm->GetPage<T::CType>(attrib.file, attrib.first_page),
				e.bm->GetPage<VarCharFixed256>(prim.file, prim.first_page)
			);
			break;
		default:
			break;
		}
	}
		break;
	case SQLTypeID<SQLString>::value:
	{
		using T = VarCharFixed256;
		switch (prim.type)
		{
		case SQLTypeID<SQLBigInt>::value:
			attrib.first_index = bp.Cast<T, SQLBigInt::CType>().BuildIndex(
				e.bm->GetPage<T>(attrib.file, attrib.first_page),
				e.bm->GetPage<SQLBigInt::CType>(prim.file, prim.first_page)
			);
			break;
		case SQLTypeID<SQLDouble>::value:
			attrib.first_index = bp.Cast<T, SQLDouble::CType>().BuildIndex(
				e.bm->GetPage<T>(attrib.file, attrib.first_page),
				e.bm->GetPage<SQLDouble::CType>(prim.file, prim.first_page)
			);
			break;
		case SQLTypeID<SQLTimeStamp>::value:
			attrib.first_index = bp.Cast<T, SQLTimeStamp::CType>().BuildIndex(
				e.bm->GetPage<T>(attrib.file, attrib.first_page),
				e.bm->GetPage<SQLTimeStamp::CType>(prim.file, prim.first_page)
			);
			break;
		case SQLTypeID<SQLString>::value:
			attrib.first_index = bp.Cast<T, VarCharFixed256>().BuildIndex(
				e.bm->GetPage<T>(attrib.file, attrib.first_page),
				e.bm->GetPage<VarCharFixed256>(prim.file, prim.first_page)
			);
			break;
		default:
			break;
		}
	}
		break;
	default:
		break;
	}
}

void ExecuteDropIndex(ExecuteContext& e, QueryClause& q, Result& r) {
	ExplodeNamesToColumnInfo(e, &q.index, q.create_index);
	//TODO: Deduce table name
	auto& meta = e.cat->FetchTable(q.index.db_name, q.index.table_name);
	for (auto attrib : meta.attributes) {
		if (attrib.index_name == q.index.column_name) {
			attrib.first_index = 0;
			attrib.index_name = "";
			break;
		}
	}
}

void ExecuteDelete(ExecuteContext& e, QueryClause& q, Result& r) {
	auto vec = Explode(q.vectables[0].name, '.');
	std::string tb = vec.back(); vec.pop_back();
	std::string db;
	if (vec.size()) {
		db = vec.back();
	}
	else {
		db = e.db;
	}
	auto& meta = e.cat->FetchTable(db, tb);
	for (auto attrib : meta.attributes) {
		q.select.push_back(ColumnName{
			db,
			tb,
			attrib.column_name });
	}
	DeduceColumnInfo(e, q);
	
	Filter(e, q, r);
}


struct ImpossibleError__Dummy {

};
std::shared_ptr<Result> Execute(ExecuteContext& ec, QueryClause& q) {
	std::shared_ptr<Result> res(new Result);
	res->ok = true;
	try {
		auto beg = std::chrono::steady_clock::now();
		switch (q.type)
		{
		case Token::kSelect:
			ExecuteSelect(ec, q, *(res.operator->()));
			break;
		case Token::kInsert:
			ExecuteInsert(ec, q, *(res.operator->()));
			break;
		case Token::kCreate:
			if (q.type_spec == Token::kTable) {
				ExecuteCreateTable(ec, q, *(res.operator->()));
				break;
			}
			if (q.type_spec == Token::kIndex) {
				ExecuteCreateIndex(ec, q, *(res.operator->()));
				break;
			}
			break;
		case Token::kUse:
			ec.db = q.default_db_name;
			break;
		case Token::kDrop:
			ExecuteDropIndex(ec, q, *(res.operator->()));
			break;
		case Token::kDelete:
			ExecuteDelete(ec, q, *(res.operator->()));
			break;
		default:
			break;
		}
		
		res->span = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - beg);
	}
#ifdef RELEASE // we dont want to catch anything in dev phase
	catch (std::exception& e) {
		res->ok = false;
		if (e.what() == nullptr) {
			return res;
		}
		try {
			console.fatal(e.what());
		}
		catch (...) {

		}
	}
#endif // RELEASE
	catch (ImpossibleError__Dummy& ) {

	}
	
	
	return res;
}