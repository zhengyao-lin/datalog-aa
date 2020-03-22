#include "DatalogAAPass.h"
#include "DatalogIR.h"
#include "Z3Backend.h"

#define DEBUG_TYPE "datalog-aa"

using namespace llvm;
using namespace std;

#include "DatalogDSL.h"

static StandardDatalog::Program program = BEGIN
    // a simple program for path finding

    sort(V, 65536);

    rel(vertex, V);
    rel(edge, V, V);
    rel(path, V, V);
    var(x); var(y); var(z);

    path(x, x) <<= vertex(x);
    path(x, y) <<= edge(x, y);
    path(x, z) <<= path(x, y) & path(y, z);

    fact vertex(1);
    fact vertex(2);
    fact vertex(3);

    fact edge(1, 2);
    fact edge(2, 3);
END;

#include "DatalogDSL.h" // toggle dsl off

DatalogAAResult::DatalogAAResult(const llvm::Module &module): module(&module) {
    Z3Backend backend;

    std::cerr << program << std::endl;

    backend.load(program);

    StandardDatalog::FormulaVector results = backend.query("path");

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
