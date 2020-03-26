#pragma once

#include <map>
#include <set>
#include <vector>

#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

#include "DatalogIR.h"
#include "Analysis/Specifications/Common.datalog"

/**
 * FactGenerator manages the mapping between values and
 * object index and generates facts used for analysis
 */
class FactGenerator {
    const llvm::Module *unit;

    // an ID that uniquely identifies an llvm::Value
    std::map<const llvm::Value *, unsigned int> valueToObjectID;

    // NOTE: the actual object id of the values in this list
    // is offsetted by NUM_SPECIAL_OBJECTS
    std::vector<const llvm::Value *> valueList;

    std::set<const llvm::Constant *> initializedConstants;

    // relations required in the program
    #define sort(name, size) std::string name = #name;
    #define rel(name, ...) \
        StandardDatalog::Relation rel_##name = StandardDatalog::Relation(#name, __VA_ARGS__)

    #define IN_DSL

    #include "Analysis/Specifications/Relations.datalog"

    #undef sort
    #undef rel
    #undef IN_DSL

public:
    FactGenerator(const llvm::Module &unit): unit(&unit) {
        initObjectIDForModule(unit);
    }

    bool isValidObjectID(unsigned int id) {
        return id - NUM_SPECIAL_OBJECTS < valueList.size();
    }

    unsigned int getObjectIDOfValue(const llvm::Value *value) {
        assert(valueToObjectID.find(value) != valueToObjectID.end() &&
               "value does not exist");
        return valueToObjectID.at(value);
    }

    const llvm::Value *getValueOfObjectID(unsigned int id) {
        unsigned int index = id - NUM_SPECIAL_OBJECTS;

        assert(index < valueList.size() &&
               "object id does not exist");
        return valueList[index];
    }

    inline unsigned int getAffiliatedObjectID(unsigned int base, unsigned idx) {
        assert(idx != 0 && "the 0th affiliated object is the base itself");
        return base + idx;
    }

    // append all the facts to the given program
    void generateFacts(StandardDatalog::Program &program) {
        generateFactsForModule(program, *unit);
    }

private:
    /**
     * initialize all objects in the current translation unit
     */
    void initObjectIDForModule(const llvm::Module &unit);
    void initObjectIDForFunction(const llvm::Function &function);
    void initObjectIDForBasicBlock(const llvm::BasicBlock &block);
    void initObjectIDForConstant(const llvm::Constant &constant);

    unsigned int getAffiliatedObjectCountForInstruction(const llvm::Instruction &instr);

    /**
     * unsigned int affiliated:
     *     Some memory objects are not represented by any llvm Value,
     *     like the actual frame object allocated by alloca.
     *     But we do need to distinguish them, so we are adding them
     *     with the operand offsetted by some number
     */
    unsigned int addValue(const llvm::Value *value, unsigned int affiliated) {
        if (valueToObjectID.find(value) != valueToObjectID.end()) {
            return valueToObjectID.at(value);
        }

        // offset by the number of special objects
        unsigned int id = valueList.size() + NUM_SPECIAL_OBJECTS;
        valueList.push_back(value);
        valueToObjectID[value] = id;

        for (unsigned int i = 0; i < affiliated; i++) {
            valueList.push_back(NULL);
        }

        return id;
    }

    unsigned int addValue(const llvm::Value *value) {
        return addValue(value, 0);
    }

    bool containPointer(const llvm::Type *type);

    void generateFactsForModule(StandardDatalog::Program &program, const llvm::Module &unit);
    void generateFactsForFunction(StandardDatalog::Program &program, const llvm::Function &function);
    void generateFactsForBasicBlock(StandardDatalog::Program &program, const llvm::BasicBlock &block);
    void generateFactsForInstruction(StandardDatalog::Program &program, const llvm::User &instr);
    void generateFactsForGlobalVariable(StandardDatalog::Program &program, const llvm::GlobalVariable &global);
    void generateFactsForConstant(StandardDatalog::Program &program, const llvm::Constant &constant);
};
