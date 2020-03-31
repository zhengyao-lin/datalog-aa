#pragma once

#include <memory>

#include "z3++.h"

#include "DatalogIR.h"

class Z3Backend: public StandardDatalog::Backend {
    std::unique_ptr<z3::context> context;
    std::unique_ptr<z3::fixedpoint> fixedpoint;
    StandardDatalog::Program program;

    std::map<std::string, z3::sort> sort_table;
    std::map<std::string, z3::func_decl> relation_table;

    unsigned int var_counter;
public:
    virtual ~Z3Backend() {
        Z3_finalize_memory();
    }

    virtual void load(const StandardDatalog::Program &program) override;
    virtual bool query(const StandardDatalog::Formula &formula) override;
    virtual std::vector<StandardDatalog::Formula> query(const std::string &relation_name) override;

private:
    static unsigned int log2(unsigned int x);

    /**
     * Compute the minimum sort required populate
     * the sort table
     */
    void initSortTable();

    /**
     * Register all relations declared in the program
     */
    void initRelationTable();

    /**
     * Same as emitFormula, but ignores the body
     */
    z3::expr emitAtom(std::map<std::string, z3::expr> &var_table,
                      const StandardDatalog::Formula &atom);

    z3::expr emitFormula(const StandardDatalog::Formula &formula);

    /**
     * Collect variables in formula and create z3 sorts/variables for them
     */
    void collectVariablesInFormula(std::map<std::string, z3::expr> &var_table,
                                   const StandardDatalog::Formula &formula);

    void collectVariablesInTerm(std::map<std::string, z3::expr> &var_table,
                                const StandardDatalog::Formula &parent,
                                unsigned int index);

    /**
     * Parse a conjunction as a list of terms
     */
    StandardDatalog::TermVector parseAssignment(z3::expr assignment_clause);
};
