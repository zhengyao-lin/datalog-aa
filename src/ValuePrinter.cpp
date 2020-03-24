#include <sstream>
#include <cassert>

#include "llvm/IR/Argument.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ModuleSlotTracker.h"

#include "ValuePrinter.h"

using namespace llvm;

namespace llvm {
    // copied from lib/IR/AsmWriter.cpp
    class SlotTracker {
    public:
        int getGlobalSlot(const GlobalValue *V);
    };
}

void ValuePrinter::printSlotNumber(llvm::raw_ostream &out, const llvm::Module *unit, const llvm::GlobalValue *value) {
    assert(!value->hasName());
    // can't use a stack object here since we don't
    // know the layout of SlotTracker
    ModuleSlotTracker module_tracker(unit);
    SlotTracker *tracker = module_tracker.getMachine();
    out << "@" << tracker->getGlobalSlot(value);
    
    // do we need to delete this?
    // delete tracker;
}

void ValuePrinter::printSlotNumber(llvm::raw_ostream &out, const llvm::Function *function, const llvm::Value *value) {
    assert(!value->hasName());
    ModuleSlotTracker tracker(function->getParent());
    tracker.incorporateFunction(*function);
    out << "%" << tracker.getLocalSlot(value);
}

void ValuePrinter::printNamedVariable(llvm::raw_ostream &out, const llvm::Value *value) {
    assert(value->hasName());

    if (auto *global = dyn_cast<GlobalValue>(value)) {
        out << "@" << value->getName();
    } else {
        out << "%" << value->getName();
    }
}

void ValuePrinter::printUniqueName(raw_ostream &out, const Value *value) {
    /**
     * types of values (https://llvm.org/doxygen/classllvm_1_1Value.html):
     *    1. argument
     *    2. instruction
     *    3. constant
     *       3.a. global
     *       3.b. constant (data/expr/aggregate)
     *       3.c. block address (?)
     *   x4. basic block
     *   x5. inline::asm
     * 
     * x: we don't need these yet
     */

    if (auto *arg = dyn_cast<Argument>(value)) {
        const Function *function = arg->getParent();
        printUniqueName(out, function);
        out << "::";

        if (value->hasName()) {
            printNamedVariable(out, value);
        } else {
            printSlotNumber(out, function, value);
        }
    } else if (auto *instr = dyn_cast<Instruction>(value)) {
        const Function *function = instr->getParent()->getParent();
        printUniqueName(out, function);
        out << "::";

        if (value->hasName()) {
            printNamedVariable(out, value);
        } else {
            printSlotNumber(out, function, value);
        }
    } else if (auto *global = dyn_cast<GlobalValue>(value)) {
        const Module *unit = global->getParent();

        if (value->hasName()) {
            printNamedVariable(out, value);
        } else {
            printSlotNumber(out, unit, global);
        }
    } else {
        // not recognized, fall back to built-in printing
        out << "<llvm ";
        value->print(out);
        out << ">";
    }
}
