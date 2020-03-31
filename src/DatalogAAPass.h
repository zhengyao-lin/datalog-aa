// this is a wrapper pass of the analysis for llvm

#pragma once

#include <map>

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Pass.h"

#include "Analysis/FactGenerator.h"

class DatalogAAResult: public llvm::AAResultBase<DatalogAAResult> {
public:
    enum Algorithm {
        Andersen
    };

private:
    static std::map<Algorithm, StandardDatalog::Program> analysis_map;

    const llvm::Module *unit;
    FactGenerator fact_generator;
    std::unique_ptr<StandardDatalog::Backend> backend; // TODO: support different backends?

    template<typename T>
    using ConcreteBinaryRelation = std::set<std::pair<T, T>>;

    ConcreteBinaryRelation<unsigned int> points_to_relation;
    ConcreteBinaryRelation<unsigned int> alias_relation;

public:
    DatalogAAResult(const llvm::Module &unit);

    llvm::AliasResult alias(const llvm::MemoryLocation &location_a, const llvm::MemoryLocation &location_b);

private:
    ConcreteBinaryRelation<unsigned int>
    getConcreteRelation(const StandardDatalog::FormulaVector &relation);

    void printPointsTo(llvm::raw_ostream &os);

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
