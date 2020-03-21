#include "DatalogAAPass.h"
#include "DatalogIR.h"
#include "Z3Backend.h"

#define DEBUG_TYPE "datalog-aa"

using namespace llvm;
using namespace std;

DatalogAAResult::DatalogAAResult(const llvm::Module &module): module(&module) {
    // test code goes here

    StandardDatalog::Sort point_sort(std::string("point"), 10);

    std::vector<std::string> domain = { "point", "point" }; 
    StandardDatalog::Relation path_relation(std::string("path"), domain);

    StandardDatalog::Program program;

    program.addSort(point_sort);
    program.addRelation(path_relation);

    std::vector<StandardDatalog::Term> terms1 = { StandardDatalog::Term(1), StandardDatalog::Term(2) };
    StandardDatalog::Formula form1("path", terms1);

    std::vector<StandardDatalog::Term> terms2 = { StandardDatalog::Term(2), StandardDatalog::Term(3) };
    StandardDatalog::Formula form2("path", terms2);

    std::vector<StandardDatalog::Term> body_term1 = { StandardDatalog::Term("x"), StandardDatalog::Term("y") };
    StandardDatalog::Formula body1("path", body_term1);

    std::vector<StandardDatalog::Term> body_term2 = { StandardDatalog::Term("y"), StandardDatalog::Term("z") };
    StandardDatalog::Formula body2("path", body_term2);

    std::vector<StandardDatalog::Term> terms3 = { StandardDatalog::Term("x"), StandardDatalog::Term("z") };
    std::vector<StandardDatalog::Formula> sub_term1 = { body1, body2 };
    StandardDatalog::Formula form3("path", terms3, sub_term1);

    program.addFormula(form1);
    program.addFormula(form2);
    program.addFormula(form3);

    Z3Backend backend;

    backend.load(program);

    std::vector<StandardDatalog::Term> terms4 = { StandardDatalog::Term(1), StandardDatalog::Term(3) };
    StandardDatalog::FormulaVector results = backend.getFixpointOf("path");

    std::cerr << program << std::endl;

    for (auto result: results) {
        std::cerr << result << std::endl;
    }
}

AliasResult DatalogAAResult::alias(const MemoryLocation &location_a, const MemoryLocation &location_b) {
    return MustAlias;
}

char DatalogAAPass::ID = 0;

// to use only this analysis, run opt with `-disable-basic -datalog-aa`
static RegisterPass<DatalogAAPass> X(
	"datalog-aa", "Alias analysis using datalog",
	false, true	
);
