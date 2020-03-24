// this is a wrapper pass of the analysis for llvm

#pragma once

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Pass.h"

#include "Analysis/FactGenerator.h"

class DatalogAAResult: public llvm::AAResultBase<DatalogAAResult> {
    const llvm::Module *unit;
    FactGenerator fact_generator;

public:
    DatalogAAResult(const llvm::Module &unit);

    llvm::AliasResult alias(const llvm::MemoryLocation &location_a, const llvm::MemoryLocation &location_b);

private:
    /**
     * Looks up and prints an object id in a readable format
     * Result of this will also be used in testing
     */
    void printObjectID(llvm::raw_ostream &os, unsigned int id);
};

class DatalogAAPass: public llvm::ExternalAAWrapperPass {
    std::unique_ptr<DatalogAAResult> result;
    
public:
    static char ID;

    DatalogAAPass(): llvm::ExternalAAWrapperPass() {
        CB = [this](llvm::Pass &, llvm::Function &, llvm::AAResults &results) {
            results.addAAResult(*result);
        };
    }

    bool doInitialization(llvm::Module &unit) override {
        result.reset(new DatalogAAResult(unit));
        return false;
    }

    bool doFinalization(llvm::Module &) override {
        result.reset();
        return false;
    }
};
