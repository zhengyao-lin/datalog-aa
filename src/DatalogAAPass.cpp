#include "llvm/Support/CommandLine.h"

#include "DatalogAAPass.h"
#include "DatalogIR.h"
#include "Z3Backend.h"
#include "ValuePrinter.h"

#define DEBUG_TYPE "datalog-aa"

using namespace llvm;
using namespace std;

#include "DatalogDSL.h"

static StandardDatalog::Program andersen = BEGIN
    #include "Analysis/Specifications/Andersen.datalog"
END;

#include "DatalogDSL.h" // toggle dsl off

cl::opt<bool> optionPrintPtsTo(
    "datalog-aa-print-pts-to", cl::NotHidden,
    cl::desc("Print the entire (may) points-to relation"),
    cl::init(true)
);

DatalogAAResult::DatalogAAResult(const llvm::Module &unit):
    unit(&unit), fact_generator(unit) {
    Z3Backend backend;
    
    StandardDatalog::Program program = andersen;

    fact_generator.generateFacts(program);
    
    // dbgs() << "================== program\n";
    // dbgs() << program << "\n";
    // dbgs() << "================== program\n";

    backend.load(program);

    StandardDatalog::FormulaVector results = backend.query("pointsTo");

    if (optionPrintPtsTo.getValue()) {
        dbgs() << "================== all addressable objects\n";

        for (auto result: results) {
            unsigned int value_id = result.getArgument(1).getValue();

            if (result.getArgument(0).getValue() == ANY_OBJECT) {
                printObjectID(dbgs(), value_id);
                dbgs() << "\n";
            }
        }

        dbgs() << "================== all addressable objects\n";

        dbgs() << "================== points-to relation\n";

        for (auto result: results) {
            unsigned int pointer_id = result.getArgument(0).getValue();
            unsigned int value_id = result.getArgument(1).getValue();

            if (pointer_id != ANY_OBJECT) {
                // dbgs() << result << " <=> ";
                printObjectID(dbgs(), pointer_id);
                dbgs() << " -> ";
                printObjectID(dbgs(), value_id);
                dbgs() << "\n";
            }
        }

        dbgs() << "================== points-to relation\n";
    }
}

void DatalogAAResult::printObjectID(raw_ostream &os, unsigned int id) {
    if (id < NUM_SPECIAL_OBJECTS) {
        switch (id) {
            case ANY_OBJECT: os << "any"; break;
            default: os << "special(" << id << ")";
        }
    } else if (fact_generator.isValidObjectID(id)) {
        const llvm::Value *value = fact_generator.getValueOfObjectID(id);

        if (value != NULL) {
            ValuePrinter::printUniqueName(os, value);
        } else {
            // for affiliated objects, try to find
            // the original object. not very efficient
            // but it will do for debugging
            unsigned int op = 0;

            do {
                op++; id--;

                const llvm::Value *value = fact_generator.getValueOfObjectID(id);

                if (value != NULL) {
                    ValuePrinter::printUniqueName(os, value);
                    os << "::aff(" << op << ")";
                    return;
                }
            } while (id >= NUM_SPECIAL_OBJECTS);
        }
    } else {
        assert(0 && "dangling affiliated object");
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
