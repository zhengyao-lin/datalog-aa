#include "llvm/Support/CommandLine.h"

#include "DatalogAAPass.h"
#include "DatalogIR.h"
#include "ValuePrinter.h"
#include "Z3Backend.h"

#define DEBUG_TYPE "datalog-aa"

using namespace llvm;
using namespace std;

static cl::opt<bool> optionPrintProgram(
    "datalog-aa-print-program", cl::NotHidden,
    cl::desc("Print the datalog program generated"),
    cl::init(false)
);

static cl::opt<bool> optionPrintPointsTo(
    "datalog-aa-print-points-to", cl::NotHidden,
    cl::desc("Print the entire (may) points-to relation"),
    cl::init(true)
);

static cl::opt<DatalogAAResult::Algorithm> optionAlgorithm(
    "datalog-aa-algorithm", cl::NotHidden,
    cl::desc("Choose the analysis algorithm to use"),
    cl::init(DatalogAAResult::Andersen),
    cl::values(
        clEnumValN(DatalogAAResult::Andersen, "andersen", "Andersen's inclusion-based analysis")
        // more to add
    )
);

#include "DatalogDSL.h"

/**
 * Load analysis programs to be referenced by their names
 * (currently only andersen's is supported)
 */
std::map<DatalogAAResult::Algorithm, StandardDatalog::Program>
DatalogAAResult::analysisMap = {
    {
        DatalogAAResult::Andersen,
        BEGIN
            #include "Analysis/Andersen.datalog"
        END
    }
};

#include "DatalogDSL.h" // toggle dsl off

char DatalogAAPass::ID = 0;

/**
 * To use this analysis only, run opt with `-disable-basicaa -datalog-aa`
 */
static RegisterPass<DatalogAAPass> X(
	"datalog-aa", "Alias analysis using datalog",
	false, true	
);

DatalogAAResult::DatalogAAResult(const llvm::Module &unit):
    unit(&unit), backend(new Z3Backend()), factGenerator(unit) {
    StandardDatalog::Program program = analysisMap[optionAlgorithm.getValue()];

    factGenerator.generateFacts(program);

    backend->load(program);

    if (optionPrintProgram.getValue()) {
        dbgs() << "================== program\n";
        dbgs() << program << "\n";
        dbgs() << "================== program\n";
    }

    // fetch points to relation
    StandardDatalog::FormulaVector points_to = backend->query("pointsTo");
    DatalogAAResult::ConcreteBinaryRelation<unsigned int> concrete_points_to = getConcreteRelation(points_to);
    pointsToRelation.swap(concrete_points_to);

    // fetch alias relation
    StandardDatalog::FormulaVector alias = backend->query("alias");
    DatalogAAResult::ConcreteBinaryRelation<unsigned int> concrete_alias = getConcreteRelation(alias);
    aliasRelation.swap(concrete_alias);

    if (optionPrintPointsTo.getValue()) {
        printPointsTo(dbgs());
    }

    // record points to set
    for (auto pair: pointsToRelation) {
        pointsToSet[pair.first].insert(pair.second);
    }
}

AliasResult DatalogAAResult::alias(const MemoryLocation &location_a, const MemoryLocation &location_b) {
    const Value *val_a = location_a.Ptr;
    const Value *val_b = location_b.Ptr;

    assert(factGenerator.hasValue(val_a) && "value does not exist");
    assert(factGenerator.hasValue(val_b) && "value does not exist");

    unsigned int val_a_id = factGenerator.getObjectIDOfValue(val_a);
    unsigned int val_b_id = factGenerator.getObjectIDOfValue(val_b);

    if (val_a_id == val_b_id) {
        return MustAlias;
    }

    // should we fallthrough to other analysis?
    std::pair<unsigned int, unsigned int> pair = std::make_pair(
        val_a_id, val_b_id
    );

    if (aliasRelation.find(pair) != aliasRelation.end()) {
        return MayAlias;
    } else {
        return NoAlias;
    }
}

bool DatalogAAResult::pointsToConstantMemory(const llvm::MemoryLocation &loc, bool or_local) {
    const Value *val = loc.Ptr;

    if (dyn_cast<Function>(val)) {
        return true;
    }

    if (const GlobalVariable *var = dyn_cast<GlobalVariable>(val)) {
        return var->isConstant();
    }

    assert(factGenerator.hasValue(val) && "value does not exist");
    unsigned int val_id = factGenerator.getObjectIDOfValue(val);

    const std::set<unsigned int> &pts_to_set = pointsToSet[val_id];

    for (unsigned int pointee: pts_to_set) {
        const Value *pointee_val = factGenerator.getMainValueOfAffiliatedObjectID(pointee);

        if (const GlobalVariable *var = dyn_cast<GlobalVariable>(pointee_val)) {
            if (!var->isConstant()) {
                return false;
            }
        } else if (dyn_cast<Function>(pointee_val)) {
            continue;
        } else if (!or_local || !dyn_cast<AllocaInst>(pointee_val)) {
            return false;
        }
    }

    return true;
}

/**
 * Converts a form vector to a concrete (binary) relation
 */
DatalogAAResult::ConcreteBinaryRelation<unsigned int>
DatalogAAResult::getConcreteRelation(const StandardDatalog::FormulaVector &relation) {
    DatalogAAResult::ConcreteBinaryRelation<unsigned int> concrete_relation;

    for (const StandardDatalog::Formula &pair: relation) {
        std::pair<unsigned int, unsigned int> item = std::make_pair(
            pair.getArgument(0).getValue(),
            pair.getArgument(1).getValue()
        );

        concrete_relation.insert(item);
    }

    return concrete_relation;
}

/**
 * Note: some tests depends on the format of this output
 */
void DatalogAAResult::printPointsTo(llvm::raw_ostream &os) {
    os << "================== all addressable objects\n";

    for (auto pair: pointsToRelation) {
        unsigned int value_id = pair.second;

        if (pair.first == ANY_OBJECT) {
            printObjectID(os, value_id);
            os << "\n";
        }
    }

    os << "================== all addressable objects\n";

    os << "================== points-to relation\n";

    for (auto pair: pointsToRelation) {
        unsigned int pointer_id = pair.first;
        unsigned int value_id = pair.second;

        if (pointer_id != ANY_OBJECT) {
            // os << result << " <=> ";
            printObjectID(os, pointer_id);
            os << " -> ";
            printObjectID(os, value_id);
            os << "\n";
        }
    }

    os << "================== points-to relation\n";
}

void DatalogAAResult::printObjectID(raw_ostream &os, unsigned int id) {
    if (id < NUM_SPECIAL_OBJECTS) {
        switch (id) {
            case ANY_OBJECT: os << "any"; break;
            default: os << "special(" << id << ")";
        }
    } else if (factGenerator.isValidObjectID(id)) {
        const llvm::Value *value = factGenerator.getValueOfObjectID(id);

        if (value != NULL) {
            ValuePrinter::printUniqueName(os, value);
        } else {
            // for affiliated objects, try to find
            // the original object. not very efficient
            // but it will do for debugging
            unsigned int op = 0;

            do {
                op++; id--;

                const llvm::Value *value = factGenerator.getValueOfObjectID(id);

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
