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
DatalogAAResult::analysis_map = {
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
    unit(&unit), backend(new Z3Backend()), fact_generator(unit) {
    StandardDatalog::Program program = analysis_map[optionAlgorithm.getValue()];

    fact_generator.generateFacts(program);

    backend->load(program);

    if (optionPrintProgram.getValue()) {
        dbgs() << "================== program\n";
        dbgs() << program << "\n";
        dbgs() << "================== program\n";
    }

    // fetch points to relation
    StandardDatalog::FormulaVector points_to = backend->query("pointsTo");
    DatalogAAResult::ConcreteBinaryRelation<unsigned int> concrete_points_to = getConcreteRelation(points_to);
    points_to_relation.swap(concrete_points_to);

    // fetch alias relation
    StandardDatalog::FormulaVector alias = backend->query("alias");
    DatalogAAResult::ConcreteBinaryRelation<unsigned int> concrete_alias = getConcreteRelation(alias);
    alias_relation.swap(concrete_alias);

    if (optionPrintPointsTo.getValue()) {
        printPointsTo(dbgs());
    }
}

AliasResult DatalogAAResult::alias(const MemoryLocation &location_a, const MemoryLocation &location_b) {
    const Value *val_a = location_a.Ptr;
    const Value *val_b = location_b.Ptr;

    assert(fact_generator.hasValue(val_a) && "value does not exist");
    assert(fact_generator.hasValue(val_b) && "value does not exist");

    unsigned int val_a_id = fact_generator.getObjectIDOfValue(val_a);
    unsigned int val_b_id = fact_generator.getObjectIDOfValue(val_b);

    dbgs() << "querying alias: ";
    val_a->print(dbgs());
    dbgs() << " vs ";
    val_b->print(dbgs());
    dbgs() << "\n";

    // should we fallthrough to other analysis?
    std::pair<unsigned int, unsigned int> pair = std::make_pair(
        val_a_id, val_b_id
    );

    if (alias_relation.find(pair) != alias_relation.end()) {
        return MayAlias;
    } else {
        return NoAlias;
    }
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

    for (auto pair: points_to_relation) {
        unsigned int value_id = pair.second;

        if (pair.first == ANY_OBJECT) {
            printObjectID(os, value_id);
            os << "\n";
        }
    }

    os << "================== all addressable objects\n";

    os << "================== points-to relation\n";

    for (auto pair: points_to_relation) {
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
