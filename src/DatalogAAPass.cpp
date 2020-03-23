#include "DatalogAAPass.h"
#include "DatalogIR.h"
#include "Z3Backend.h"

#define DEBUG_TYPE "datalog-aa"

using namespace llvm;
using namespace std;

#include "DatalogDSL.h"

static StandardDatalog::Program andersen = BEGIN
    #include "Analysis/Specifications/Andersen.datalog"
END;

#include "DatalogDSL.h" // toggle dsl off

DatalogAAResult::DatalogAAResult(const llvm::Module &unit):
    unit(&unit), fact_generator(unit) {
    Z3Backend backend;
    
    StandardDatalog::Program program = andersen;

    fact_generator.generateFacts(program);
    
    dbgs() << "================== program\n";
    dbgs() << program << "\n";
    dbgs() << "================== program\n";

    backend.load(program);

    StandardDatalog::FormulaVector results = backend.query("pointsTo");

    dbgs() << "================== results\n";

    for (auto result: results) {
        unsigned int pointer_id = result.getArgument(0).getValue();
        unsigned int value_id = result.getArgument(1).getValue();

        dbgs() << result << " <=> ";
        fact_generator.printObjectID(dbgs(), pointer_id);
        dbgs() << " points to ";
        fact_generator.printObjectID(dbgs(), value_id);
        dbgs() << "\n";
    }

    dbgs() << "================== results\n";
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
