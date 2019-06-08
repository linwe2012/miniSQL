#include "execute.h"

void DeduceColumnInfo(ExecuteContext& e, QueryClause& q);

void PluginFunctions(ExecuteContext& e, QueryClause& q);

void OptimizeWhereClause(ExecuteContext& e, QueryClause& q);

void RefineWhereClause(ExecuteContext& e, QueryClause& q) {
	DeduceColumnInfo(e, q);
	PluginFunctions(e, q);
	OptimizeWhereClause(e, q);
}


void DeduceColumnInfo(ExecuteContext& e, QueryClause& q) {
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
			q.tables[UnqiueTable{ table.db_name, table.alias, true }] = &table;
		};
	}
	std::vector<QueryVariable> attribs;

	for (auto var : q.variables) {
		std::vector<std::string> names = Explode(var->name(), '.');
		if (names.size() > 3) {
			console.warn("Too many specifiers in column name: ", var->name());
		}
		ColumnName col;
		col.column_name = names.back(); names.pop_back();

		if (names.size()) {
			col.table_name = names.back(); names.pop_back();
		}

		if (names.size()) {
			col.db_name = names.back(); names.pop_back();
		}
		else {
			col.db_name = e.db;
		}

		if (col.table_name.size()) {
			auto tb = q.tables.find(UnqiueTable{ col.db_name, col.table_name });
			if (tb == q.tables.end()) {
				console.warn("Unresolved column: database: ", col.db_name, "table: ", col.table_name, "is not presented in from clause");
			}
			col.table_name = tb->second->db_name;
		}

		if (col.table_name.size() == 0) {
			std::string table_name;
			for (auto& table : q.tables) {
				if (table.first.is_alias) {
					continue;
				}
				if (col.db_name == table.first.db) {
					bool found = true;
					Attribute* attrib;
					try {
						col.table_name = table.second->name;
						attrib = &e.cat->FetchColumn(col);
					}
					catch (...) {
						// not found
					}

					if (!found) {
						continue;
					}

					if (table_name.size() && table_name != col.table_name) {
						console.warn("Unable to resolve ambiguous table: column ",
							col.column_name, " has 2 possible table: ", col.table_name, " or ", table_name);
					}

					table_name = col.table_name;
				}
			}
		}

		auto qvar = QueryVariable{ &e.cat->FetchColumn(col) };
		var->Bind(attribs.size(), qvar.attrib->type);
		attribs.push_back(qvar);
	}

	q.variable_attribs = std::move(attribs);
}

void PluginFunctions(ExecuteContext& e, QueryClause& q)
{
	for (auto f : q.functions) {
		auto ptr = e.fun->Get(f->function_name());
		if (ptr == nullptr) {
			console.fatal("Undecalred function: ", f->function_name());
		}

		if (ptr->num_params >= 0) {
			if (ptr->num_params != f->num_params) {
				console.fatal("Function ", f->function_name(), "requires ",
					f->num_params, " paramters", ", yet", ptr->num_params, "is provided");
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

#define DEF_CNT(Name, literal, prec) int Name;
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
			int level = (trace.size() > 0) ? 0 : (trace.back().level + 1);

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
		if (!(constant || var)) {
			constant = c->rhs()->AsConstantDataOracle();
			var = c->lhs()->AsVariableOracle();
		}
		if (!(constant || var)) {
			return;
		}

		auto& qvar = q.variable_attribs[var->id()];
		if (qvar.attrib->first_index.IsNil()) {
			return;
		}

		if (c->op() == Token::kEqual) {
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

	friend class Tracer;
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


void Filter(ExecuteContext& e, QueryClause& q) {

}

void FilterRoll(ExecuteContext& e, QueryClause& q, IOracle::Iterators itrs, int id) {
	int next = id;;
	if (q.variable_attribs[id].index_method == QueryVariable::kDirectIndex) {
		for (next = id + 1; next < q.variable_attribs.size(); ++next) {
			FilterRoll(e, q, itrs, next);
		}
	}
	while (itrs[id].IsNil())
	{

	}
}