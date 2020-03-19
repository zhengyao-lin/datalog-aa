// this is a wrapper pass of the analysis for llvm

#pragma once

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Pass.h"

class DatalogAAResult : public llvm::AAResultBase<DatalogAAResult> {
    const llvm::Module *module;

public:
    DatalogAAResult(const llvm::Module &module): module(&module) {}

    llvm::AliasResult alias(const llvm::MemoryLocation &location_a, const llvm::MemoryLocation &location_b);
};

class DatalogAAPass : public llvm::ExternalAAWrapperPass {
    std::unique_ptr<DatalogAAResult> result;
    
public:
    static char ID;

    DatalogAAPass() : llvm::ExternalAAWrapperPass() {
        CB = [this](llvm::Pass &, llvm::Function &, llvm::AAResults &results) {
            results.addAAResult(*result);
        };
    }

    bool doInitialization(llvm::Module &module) override {
        result.reset(new DatalogAAResult(module));
        return false;
    }

    bool doFinalization(llvm::Module &) override {
        result.reset();
        return false;
    }
};
