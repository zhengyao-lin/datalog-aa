#pragma once

#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"

class ValuePrinter {
public:
    // uniqueness is based on a best-effort basis
    static void printUniqueName(llvm::raw_ostream &out, const llvm::Value *value);

private:
    static void printSlotNumber(llvm::raw_ostream &out, const llvm::Module *unit, const llvm::GlobalValue *value);
    static void printSlotNumber(llvm::raw_ostream &out, const llvm::Function *function, const llvm::Value *value);
    static void printNamedVariable(llvm::raw_ostream &out, const llvm::Value *value);
};
