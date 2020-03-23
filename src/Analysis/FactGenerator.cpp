#include "llvm/IR/Function.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"

#include "FactGenerator.h"

using namespace llvm;

void FactGenerator::printObjectID(raw_ostream &os, unsigned int id) {
    if (id < NUM_SPECIAL_OBJECTS) {
        os << "<special " << id << ">";
    } else if ((id - NUM_SPECIAL_OBJECTS) < valueList.size()) {
        const llvm::Value *value = getValueOfObjectID(id);

        if (value != NULL) {
            os << "<llvm: ";
            value->print(os);
            os << ">";
        } else {
            os << "<affiliated " << id << ">";
        }
    } else {
        os << "<affiliated " << id << ">";
    }
}

void FactGenerator::initObjectIDForModule(const Module &unit) {
    for (const GlobalVariable &global: unit.globals()) {
        // we will distinguish between
        // a global variable and a global object
        // the former is a pointer to the latter
        addValue(&global, 1);

        if (global.hasInitializer()) {
            initObjectIDForConstant(*global.getInitializer());
        }
    }

    for (const Function &function: unit) {
        initObjectIDForFunction(function);
    }

    dbgs() << "================== value map\n";

    // for debug, print out everything
    for (const Value *value: valueList) {
        if (value != NULL) {
            dbgs() << "value ";
            value->print(dbgs());
            dbgs() << " -> " << getObjectIDOfValue(value) << "\n";
        }
    }

    dbgs() << "================== value map\n";
}

void FactGenerator::initObjectIDForFunction(const Function &function) {
    // the function itself is a pointer
    // while it also points to the actual content
    // of the function in memory
    addValue(&function, 1);

    for (const Argument &arg: function.args()) {
        addValue(&arg);
    }

    for (const BasicBlock &block: function) {
        initObjectIDForBasicBlock(block);
    }
}

void FactGenerator::initObjectIDForBasicBlock(const BasicBlock &block) {
    addValue(&block);

    for (const Instruction &instr: block) {
        addValue(&instr, getAffiliatedObjectCountForInstruction(instr));

        for (const Use &operand: instr.operands()) {
            addValue(operand);
        }
    }
}

void FactGenerator::initObjectIDForConstant(const Constant &constant) {
    addValue(&constant);

    // constant expr may have operands
    if (auto *expr = dyn_cast<ConstantExpr>(&constant)) {
        for (const Use &operand: expr->operands()) {
            addValue(operand);
        }
    }
}

unsigned int FactGenerator::getAffiliatedObjectCountForInstruction(const llvm::Instruction &instr) {
    switch (instr.getOpcode()) {
        case Instruction::Alloca: return 1; // alloca creates a frame object
        default: return 0;
    }
}

/**
 * For the first stage, we will limit ourselves to
 * modules with the following requirements:
 *   - (non-external) functions
 *   - only alloca, load, getelementptr, store, ret instructions
 *   - no constant expressions
 *   - no globals
 */

void FactGenerator::generateFactsForModule(StandardDatalog::Program &program, const Module &unit) {
    // TODO: leave constants alone for now

    for (const Function &function: unit) {
        generateFactsForFunction(program, function);
    }
}

void FactGenerator::generateFactsForFunction(StandardDatalog::Program &program, const Function &function) {
    unsigned int function_id = getObjectIDOfValue(&function);
    unsigned int function_mem_id = getAffiliatedObjectID(function_id, 1);

    program.addFormula(rel_function(function_id));
    program.addFormula(rel_mem(function_mem_id));
    program.addFormula(rel_hasValue(function_id, function_mem_id));

    // both function pointer and function object are immutable
    program.addFormula(rel_immutable(function_id));
    program.addFormula(rel_immutable(function_mem_id));

    // NOTE that the function pointer is non-addressable
    // but the function object itself is addressable (in particular by the pointer)
    program.addFormula(rel_nonaddressable(function_id));

    for (const Value &arg: function.args()) {
        unsigned int arg_id = getObjectIDOfValue(&arg);

        // TODO: check if this is true (variadic argument?)
        program.addFormula(rel_nonaddressable(arg_id));

        program.addFormula(rel_immutable(arg_id));
    }

    for (const BasicBlock &block: function) {
        generateFactsForBasicBlock(program, block);
    }
}

void FactGenerator::generateFactsForBasicBlock(StandardDatalog::Program &program, const BasicBlock &block) {
    for (const Instruction &instr: block) {
        generateFactsForInstruction(program, instr);
    }
}

void FactGenerator::generateFactsForInstruction(StandardDatalog::Program &program, const Instruction &instr) {
    unsigned int instr_id = getObjectIDOfValue(&instr);
    unsigned int function_id = getObjectIDOfValue(instr.getParent()->getParent());

    program.addFormula(rel_instr(instr_id));
    program.addFormula(rel_hasInstr(function_id, instr_id));

    // result of an instruction is immutable and non-addressable
    // because we are in SSA form
    program.addFormula(rel_immutable(instr_id));
    program.addFormula(rel_nonaddressable(instr_id));

    for (const Use &operand: instr.operands()) {
        unsigned int operand_id = getObjectIDOfValue(operand);
        program.addFormula(rel_hasOperand(instr_id, operand_id));
    }

    /**
     * Add specific instruction facts
     */

    if (auto *alloca_instr = dyn_cast<AllocaInst>(&instr)) {
        unsigned int mem_id = getAffiliatedObjectID(instr_id, 1);

        program.addFormula(rel_mem(mem_id));
        program.addFormula(rel_instrAlloca(instr_id, mem_id));

    } else if (auto *getelementptr_instr = dyn_cast<GetElementPtrInst>(&instr)) {
        const Value *base = getelementptr_instr->getOperand(0);
        unsigned int base_id = getObjectIDOfValue(base);

        program.addFormula(rel_instrGetelementptr(instr_id, base_id));

    } else if (auto *load_instr = dyn_cast<LoadInst>(&instr)) {
        const Value *src = load_instr->getOperand(0);
        unsigned int src_id = getObjectIDOfValue(src);

        program.addFormula(rel_instrLoad(instr_id, src_id));

    } else if (auto *store_instr = dyn_cast<StoreInst>(&instr)) {
        const Value *value = store_instr->getOperand(0);
        unsigned int value_id = getObjectIDOfValue(value);

        const Value *dest = store_instr->getOperand(1);
        unsigned int dest_id = getObjectIDOfValue(dest);

        program.addFormula(rel_instrStore(instr_id, value_id, dest_id));

    } else if (auto *ret_instr = dyn_cast<ReturnInst>(&instr)) {
        const Value *value = ret_instr->getOperand(0);
        unsigned int value_id = getObjectIDOfValue(value);

        program.addFormula(rel_instrRet(instr_id, value_id));
    } else {
        program.addFormula(rel_instrUnknown(instr_id));

        dbgs() << "unsupported instruction ";
        instr.print(dbgs());
        dbgs() << "\n";
    }
}

// void FactGenerator::generateFactsForGlobalVariable(StandardDatalog::Program &program, const GlobalVariable &global);
// void FactGenerator::generateFactsForConstant(StandardDatalog::Program &program, const Constant &constant);
