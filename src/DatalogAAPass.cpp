#include "DatalogAAPass.h"
#include "DatalogIR.h"
#include "Z3Backend.h"

#define DEBUG_TYPE "datalog-aa"

using namespace llvm;
using namespace std;

DatalogAAResult::DatalogAAResult(const llvm::Module &module): module(&module) {
    // test code goes here

    StandardDatalog::Program program = DATALOG_BEGIN
        DATALOG_SORT(vertex, 10);
        DATALOG_REL(edge, vertex, vertex);
        DATALOG_REL(path, vertex, vertex);
        
        DATALOG_HORN(
            DATALOG_ATOM(path, string("x"), string("y")),
            DATALOG_ATOM(edge, string("x"), string("y"))
        );

        DATALOG_HORN(
            DATALOG_ATOM(path, string("x"), string("z")),
            DATALOG_ATOM(path, string("x"), string("y")),
            DATALOG_ATOM(path, string("y"), string("z"))
        );

        DATALOG_FACT(edge, 1, 2);
        DATALOG_FACT(edge, 2, 3);
        DATALOG_FACT(edge, 3, 4);
        DATALOG_FACT(edge, 2, 5);
        DATALOG_FACT(edge, 3, 6);
    DATALOG_END;

    Z3Backend backend;

    backend.load(program);

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
