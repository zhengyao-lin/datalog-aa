#include <cassert>
#include <iostream>

#include "z3++.h"

#include "Z3Backend.h"

#define VARIABLE_PREFIX "V"
#define RULE_NAME_PREFIX "rule-"

/* reference: https://stackoverflow.com/questions/11376288/fast-computing-of-log2-for-64-bit-integers */
unsigned int Z3Backend::log2(unsigned int x) {
    static const int tab32[64] = {
        0,   9,  1, 10, 13, 21,  2, 29,
        11, 14, 16, 18, 22, 25,  3, 30,
        8,  12, 20, 28, 15, 17, 24,  7,
        19, 27, 23,  6, 26,  5,  4, 31
    };

    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    
    return tab32[(uint32_t)(x * 0x07c4acdd) >> 27];
}

void Z3Backend::initSortTable() {
    sort_table.clear();

    for (auto const &item: program.getSorts()) {
        unsigned int bit_size = log2(item.second.getSize()) + 1;
        sort_table.insert(std::make_pair(item.first, context->bv_sort(bit_size)));
    }
}

void Z3Backend::initRelationTable() {
    for (auto const &item: program.getRelations()) {
        StandardDatalog::Relation relation = item.second;

        z3::sort_vector sorts(*context);

        for (const std::string &name: relation.getArgumentSorts()) {
            auto found = sort_table.find(name);
            assert(found != sort_table.end() && "sort doesn't exist");
            sorts.push_back(found->second);
        }

        z3::func_decl function = context->function(relation.getName().c_str(), sorts, context->bool_sort());
        relation_table.insert(std::make_pair(relation.getName(), function));

        fixedpoint->register_relation(function);
    }
}

/**
 * Replace the current environment with a new program
 */
void Z3Backend::load(const StandardDatalog::Program &program) {
    context.reset(new z3::context());
    fixedpoint.reset(new z3::fixedpoint(*context));

    this->program = program;
    var_counter = 0;

    initSortTable();
    initRelationTable();

    unsigned int rule_counter = 0;

    for (auto const &formula: program.getFormulas()) {
        // scan for variables

        z3::expr rule = emitFormula(formula);

        std::string rule_name = RULE_NAME_PREFIX + formula.getName() + "-" + std::to_string(rule_counter);
        rule_counter++;

        fixedpoint->add_rule(rule, context->str_symbol(rule_name.c_str()));
    }
}

z3::expr Z3Backend::emitAtom(std::map<std::string, z3::expr> &var_table,
                             const StandardDatalog::Formula &atom) {
    std::string relation_name = atom.getName();
    const StandardDatalog::Relation &relation = program.getRelation(relation_name);

    z3::expr_vector args(*context);

    unsigned int idx = 0;

    for (auto const &term: atom.getArguments()) {
        if (term.isVariable()) {
            assert(var_table.find(term.getVariable()) != var_table.end() && "missing variable");
            
            args.push_back(var_table.at(term.getVariable()));
        } else {
            const StandardDatalog::Sort &arg_sort = relation.getArgumentSort(idx);
            unsigned int bit_size = sort_table.at(arg_sort.getName()).bv_size();
            z3::expr expr = context->bv_val(term.getValue(), bit_size);
        
            args.push_back(expr);
        }

        idx++;
    }

    z3::func_decl func = relation_table.at(relation_name);

    return func(args);
}

z3::expr Z3Backend::emitFormula(const StandardDatalog::Formula &formula) {
    // head -: f1, f2, f3, ...
    // ==> forall (vars...) (f1 /\ f2 /\ f3 /\ ...) => head
    
    // collect all (free) variables
    std::map<std::string, z3::expr> var_table;
    collectVariablesInFormula(var_table, formula);

    z3::expr head = emitAtom(var_table, formula);
    z3::expr_vector horn_body(*context);

    for (auto const &sub_term: formula.getBody()) {
        horn_body.push_back(emitAtom(var_table, sub_term));
    }

    z3::expr rule = head;

    // prepend body
    if (!horn_body.empty()) {
        rule = z3::implies(z3::mk_and(horn_body), head);
    }

    // quantify all variables
    if (!var_table.empty()) {
        z3::expr_vector vars(*context);

        for (auto const &item: var_table) {
            vars.push_back(item.second);
        }

        rule = z3::forall(vars, rule);
    }

    return rule;
}

void Z3Backend::collectVariablesInFormula(std::map<std::string, z3::expr> &var_table,
                                          const StandardDatalog::Formula &formula) {
    for (unsigned int idx = 0; idx < formula.getArguments().size(); idx++) {
        collectVariablesInTerm(var_table, formula, idx);
    }

    for (auto const &sub_form: formula.getBody()) {
        for (unsigned int idx = 0; idx < sub_form.getArguments().size(); idx++) {
            collectVariablesInTerm(var_table, sub_form, idx);
        }
    }
}

void Z3Backend::collectVariablesInTerm(std::map<std::string, z3::expr> &var_table,
                                       const StandardDatalog::Formula &parent,
                                       unsigned int index) {

    const StandardDatalog::Term &term = parent.getArgument(index);

    if (term.isVariable()) {
        std::string var = term.getVariable();

        if (var_table.find(var) == var_table.end()) {
            const StandardDatalog::Sort &arg_sort =
                program.getRelation(parent.getName()).getArgumentSort(index);

            z3::sort var_sort = sort_table.at(arg_sort.getName());

            std::string var_name = VARIABLE_PREFIX + std::to_string(var_counter);
            var_counter++;

            var_table.insert(
                std::make_pair(var, context->constant(var_name.c_str(), var_sort))
            );
        } // TODO: else, check if the types match
    }
}

StandardDatalog::FormulaVector Z3Backend::query(const std::string &relation_name) {
    assert(relation_table.find(relation_name) != relation_table.end() &&
           "relation does not exist");

    z3::func_decl relation = relation_table.at(relation_name);
    z3::func_decl_vector relations(*context);
    relations.push_back(relation);

    z3::check_result result = fixedpoint->query(relations);

    StandardDatalog::FormulaVector facts;

    if (result == z3::unsat) {
        // unsatisfiable/empty relation
        return facts;
    }

    if (result == z3::unknown) {
        std::cerr << "z3 returned unknown: "
                  << fixedpoint->reason_unknown()
                  << std::endl;
        assert(0);
    }

    // obtain the actual relation
    z3::expr relation_constraint = fixedpoint->get_answer();

    // two cases:
    // 1. a single assignment, in the form of conjunction of variables
    // 2. disjunction of multiple assignments

    if (relation_constraint.is_and() || relation_constraint.is_eq()) {
        // (and (= (:var a) <c1>) ...)
        StandardDatalog::TermVector args = parseAssignment(relation_constraint);
        StandardDatalog::Formula formula(relation_name, args);
        facts.push_back(formula);
    } else if (relation_constraint.is_or()) {
        // (or (and (= (:var a) <c1>) (= (:var b) <c2>) ...) ...)
        unsigned int num_arg = relation_constraint.num_args();

        for (unsigned int i = 0; i < num_arg; i++) {
            StandardDatalog::TermVector args = parseAssignment(relation_constraint.arg(i));
            StandardDatalog::Formula formula(relation_name, args);
            facts.push_back(formula);
        }
    } else {
        std::cerr << "relation constraint in unexpected format: "
                  << relation_constraint
                  << std::endl;
        assert(0);
    }

    return facts;
}

StandardDatalog::TermVector Z3Backend::parseAssignment(z3::expr assignment_clause) {
    StandardDatalog::TermVector args;

    if (assignment_clause.is_and()) {
        unsigned int num_arg = assignment_clause.num_args();

        for (unsigned int i = 0; i < num_arg; i++) {
            // expecting to be of the form (= (:var i) <constant>)
            z3::expr assignment = assignment_clause.arg(i);
            assert(assignment.is_eq() && assignment.num_args() == 2 &&
                    "unexpected assignment format");

            z3::expr lhs = assignment.arg(0);
            z3::expr rhs = assignment.arg(1);
        
            assert(lhs.is_var() && "lhs is not a variable");
            assert(rhs.is_bv() && "rhs is not a bit vector");
            
            unsigned int constant = rhs.get_numeral_uint();

            // TODO: this loop is relying on the order of
            // equalities in the conjunction, which may
            // not be reliable

            args.push_back(constant);
        }
    } else if (assignment_clause.is_eq()) {
        // a single equality constraint
        z3::expr lhs = assignment_clause.arg(0);
        z3::expr rhs = assignment_clause.arg(1);

        assert(lhs.is_var() && "lhs is not a variable");
        assert(rhs.is_bv() && "rhs is not a bit vector");

        unsigned int constant = rhs.get_numeral_uint();

        args.push_back(constant);
    } else {
        std::cerr << "unexpected assignment: "
                  << assignment_clause
                  << std::endl;
        assert(0);
    }

    return args;
}
