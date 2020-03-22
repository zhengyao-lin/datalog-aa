#include "DatalogAAPass.h"
#include "DatalogIR.h"
#include "Z3Backend.h"

#define DEBUG_TYPE "datalog-aa"

using namespace llvm;
using namespace std;

#include "DatalogDSL.h"

static StandardDatalog::Program andersen = BEGIN
    #include "Analysis/Common.datalog"
    { // add a layer of scope to prevent variable clashing
        #include "Analysis/Andersen.datalog"
    }
END;

#include "DatalogDSL.h" // toggle dsl off

DatalogAAResult::DatalogAAResult(const llvm::Module &unit):
    unit(&unit), fact_generator(unit) {
    Z3Backend backend;
    
    StandardDatalog::Program program = andersen;

    fact_generator.generateFacts(program);
    
    std::cerr << "================== program" << std::endl;
    std::cerr << program << std::endl;
    std::cerr << "================== program" << std::endl;

    backend.load(program);

    StandardDatalog::FormulaVector results = backend.query("pointsTo");

    std::cerr << "================== results" << std::endl;

    for (auto result: results) {
        unsigned int pointer_id = result.getArgument(0).getValue();
        unsigned int value_id = result.getArgument(1).getValue();

        const llvm::Value *pointer = fact_generator.getValueOfObjectID(pointer_id);
        const llvm::Value *value = fact_generator.getValueOfObjectID(value_id);

        std::cerr << result << " <=> ";
        
        if (pointer != NULL)
            pointer->print(dbgs());
        else
            dbgs() << "<memory " << pointer_id << ">";
        
        std::cerr << " points to ";

        if (value != NULL)
            value->print(dbgs());
        else
            dbgs() << "<memory " << value_id << ">";

        std::cerr << std::endl;
    }

    std::cerr << "================== results" << std::endl;
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
