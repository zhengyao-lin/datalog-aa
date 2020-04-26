#include "llvm/IR/Function.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"

#include "ValuePrinter.h"
#include "FactGenerator.h"

using namespace llvm;

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

    // dbgs() << "================== value map\n";

    // // for debug, print out everything
    // for (const Value *value: valueList) {
    //     if (value != NULL) {
    //         dbgs() << "value ";
    //         value->print(dbgs());
    //         dbgs() << " -> " << getObjectIDOfValue(value) << "\n";
    //     }
    // }

    // dbgs() << "================== value map\n";
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
    // TODO: should we consider basic block objects
    // addValue(&block);

    for (const Instruction &instr: block) {
        addValue(&instr, getAffiliatedObjectCountForInstruction(instr));

        for (const Use &operand: instr.operands()) {
            if (auto *constant = dyn_cast<Constant>(operand)) {
                initObjectIDForConstant(*constant);
            } else {
                addValue(operand);
            }
        }
    }
}

void FactGenerator::initObjectIDForConstant(const Constant &constant) {
    addValue(&constant);

    // constant may have operands in case of
    // constant aggregate or constant expression
    for (const Use &operand: constant.operands()) {
        auto *constant_operand = dyn_cast<Constant>(operand);
        assert(constant_operand && "non-constant operand of constant value");
        initObjectIDForConstant(*constant_operand);
    }
}

unsigned int FactGenerator::getAffiliatedObjectCountForInstruction(const llvm::Instruction &instr) {
    // search for special cases for intrinsic function calls
    if (auto *call_instr = dyn_cast<CallInst>(&instr)) {
        for (auto *intrinsic: intrinsicList) {
            IntrinsicCall::MatchResult result = intrinsic->match(call_instr);

            if (result.matched) {
                return result.affiliated;
            }
        }
    }

    switch (instr.getOpcode()) {
        case Instruction::Alloca: return 1; // alloca creates a frame object
        default: return 0;
    }
}

bool FactGenerator::containPointer(const Type *type) {
    if (type->isPointerTy()) return true;

    for (const Type *subtype: type->subtypes()) {
        if (containPointer(subtype)) {
            return true;
        }
    }

    return false;
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
    initializedConstants.clear();

    // similar to alloca, a global variable has two objects associated with it
    // the variable itself, which points to the actual mem object
    for (const GlobalVariable &global: unit.globals()) {
        generateFactsForGlobalVariable(program, global);
    }

    for (const Function &function: unit) {
        generateFactsForFunction(program, function);
    }

    // TODO: it would be nice if we can
    // go through all constants here without
    // recursively finding all constants
}

void FactGenerator::generateFactsForFunction(StandardDatalog::Program &program, const Function &function) {
    generateFactsForValue(program, function);

    unsigned int function_id = getObjectIDOfValue(&function);
    unsigned int function_mem_id = getAffiliatedObjectID(function_id, 1);

    program.addFormula(rel_function(function_id));
    program.addFormula(rel_mem(function_mem_id));
    program.addFormula(rel_hasAllocatedMemory(function_id, function_mem_id));

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
    // TODO: consider addresses of basic blocks
    // unsigned int block_id = getObjectIDOfValue(&block);
    // unsigned int function_id = getObjectIDOfValue(block.getParent());

    // program.addFormula(rel_block(block_id));
    // program.addFormula(rel_immutable(block_id));
    // program.addFormula(rel_hasBlock(function_id, block_id));

    for (const Instruction &instr: block) {
        generateFactsForValue(program, instr);
        generateFactsForInstruction(program, instr);
    }
}

/**
 * Generate facts for both instructions and constant expressions
 */
void FactGenerator::generateFactsForInstruction(StandardDatalog::Program &program, const User &user) {
    unsigned int opcode;
    unsigned int instr_id = getObjectIDOfValue(&user);

    if (auto *instr = dyn_cast<Instruction>(&user)) {
        unsigned int function_id = getObjectIDOfValue(instr->getParent()->getParent());
        opcode = instr->getOpcode();
        program.addFormula(rel_hasInstr(function_id, instr_id));
    } else if (auto *expr = dyn_cast<ConstantExpr>(&user)) {
        opcode = expr->getOpcode();
    } else {
        assert(0 && "not an instruction or constant expression");
    }

    program.addFormula(rel_instr(instr_id));

    // result of an instruction is immutable and non-addressable
    // because we are in SSA form
    program.addFormula(rel_immutable(instr_id));
    program.addFormula(rel_nonaddressable(instr_id));

    for (const Use &operand: user.operands()) {
        unsigned int operand_id = getObjectIDOfValue(operand);
        program.addFormula(rel_hasOperand(instr_id, operand_id));

        // TODO: can there be other kinds of operands?

        if (auto *constant = dyn_cast<Constant>(operand)) {
            generateFactsForConstant(program, *constant);
        } else {
            assert((isa<Argument>(operand) ||
                    isa<BasicBlock>(operand) ||
                    isa<Instruction>(operand)) &&
                   "unexpected type of operand");
        }
    }

    switch (opcode) {
        case Instruction::Alloca: {
            unsigned int mem_id = getAffiliatedObjectID(instr_id, 1);
            program.addFormula(rel_mem(mem_id));
            program.addFormula(rel_instrAlloca(instr_id, mem_id));

            auto *alloca_inst = dyn_cast<AllocaInst>(&user);

            // if the allocated type is a pointer, assume it
            // doesn't point to anything in the beginning since it
            // would be an undefined behaviour to read it otherwise

            // TODO: check if this is true

            break;
        }

        case Instruction::GetElementPtr: {
            const Value *base = user.getOperand(0);
            unsigned int base_id = getObjectIDOfValue(base);
            program.addFormula(rel_instrGetelementptr(instr_id, base_id));
            break;
        }

        case Instruction::Load: {
            const Value *src = user.getOperand(0);
            unsigned int src_id = getObjectIDOfValue(src);
            program.addFormula(rel_instrLoad(instr_id, src_id));
            break;
        }

        case Instruction::Store: {
            const Value *value = user.getOperand(0);
            unsigned int value_id = getObjectIDOfValue(value);

            const Value *dest = user.getOperand(1);
            unsigned int dest_id = getObjectIDOfValue(dest);

            program.addFormula(rel_instrStore(instr_id, value_id, dest_id));
            break;
        }

        case Instruction::Ret: {
            // ignore ret void
            if (user.getNumOperands() > 0) {
                const Value *value = user.getOperand(0);
                unsigned int value_id = getObjectIDOfValue(value);
                program.addFormula(rel_instrRet(instr_id, value_id));
            }
            break;
        }

        case Instruction::BitCast: {
            const Value *value = user.getOperand(0);
            unsigned int value_id = getObjectIDOfValue(value);
            program.addFormula(rel_instrBitCast(instr_id, value_id));
            break;
        }

        // this is the ONLY place where we can
        // get a pointer out of an integer
        // (even calling memcpy would require us to convert first)
        case Instruction::IntToPtr: {
            // TODO: being most conservative right now and assume
            // this can point to anything
            const Value *value = user.getOperand(0);
            unsigned int value_id = getObjectIDOfValue(value);
            program.addFormula(rel_instrIntToPtr(instr_id, value_id));
            break;
        }

        case Instruction::PHI: {
            program.addFormula(rel_instrPHI(instr_id));
            break;
        }

        case Instruction::Br: {
            // TODO: ignore for now since we are
            // currently flow-insensitive
            break;
        }

        // TODO: case Instruction::Invoke:
        // TODO: calls to function pointers

        // this way of checking if a function has definition or not
        // comes from https://github.com/grievejia/andersen/blob/master/lib/ConstraintCollect.cpp#L351
        case Instruction::Call: {
            const CallInst *call = dyn_cast<CallInst>(&user);
            assert(call && "not a call instruction");

            const Function *function = call->getCalledFunction();

            if (!function) {
                goto UNKNOWN_INSTR;
            }
            
            unsigned int call_id = getObjectIDOfValue(call);
            unsigned int function_id = getObjectIDOfValue(function);

            if (function->isDeclaration() || function->isIntrinsic()) {
                bool matched = false;
                
                for (auto *intrinsic: intrinsicList) {
                    if (intrinsic->match(call).matched) {
                        // generate specific facts for intrinsic function calls
                        intrinsic->generate(this, program, call);
                        matched = true;
                        break;
                    }
                }

                if (!matched)
                    goto UNKNOWN_INSTR;
            } else {
                // defined in this module
                unsigned int i = 0;

                program.addFormula(rel_instrCall(call_id, function_id));

                for (const Argument &arg: function->args()) {
                    assert(i < call->getNumArgOperands() &&
                           "number of arguments does not match the number of formal arguments");

                    const Value *call_arg = call->getArgOperand(i);
                    
                    unsigned int arg_id = getObjectIDOfValue(&arg);
                    unsigned int call_arg_id = getObjectIDOfValue(call_arg);

                    program.addFormula(rel_hasCallArgument(call_id, call_arg_id, arg_id));

                    i++;
                }
            }

            break;
        }

        // whitelist
        case Instruction::ICmp: break;
        case Instruction::FCmp: break;

        // irrelavent case instructions
        case Instruction::Trunc: break;
        case Instruction::ZExt: break;
        case Instruction::SExt: break;
        case Instruction::FPToUI: break;
        case Instruction::UIToFP: break;
        case Instruction::SIToFP: break;
        case Instruction::FPTrunc: break;
        case Instruction::FPExt: break;

        case Instruction::Unreachable: break;

        default:
        UNKNOWN_INSTR:
            // ignore arithemtic/logical operators
            if (const Instruction *instr = dyn_cast<Instruction>(&user)) {
                if (instr->isBinaryOp() ||
                    instr->isUnaryOp()) {
                    break;
                }
            }

            program.addFormula(rel_instrUnknown(instr_id));
            dbgs() << "unsupported instruction ";
            user.print(dbgs());
            dbgs() << "\n";
    }
}

void FactGenerator::generateFactsForGlobalVariable(StandardDatalog::Program &program, const GlobalVariable &global) {
    generateFactsForValue(program, global);

    unsigned int global_id = getObjectIDOfValue(&global);
    unsigned int global_mem_id = getAffiliatedObjectID(global_id, 1);

    // NOTE that we need to distinguish two cases
    //   - a pointer potentially pointing to any object
    //   - a pointer (e.g. global) pointing to a unknown object
    //     which, in turn, can point to anything
    // in this case, global_var_id points to a (persumably)
    // unique location, but we just don't know its content

    program.addFormula(rel_global(global_id));

    // the pointer to a global variable is immutable
    program.addFormula(rel_immutable(global_id));
    program.addFormula(rel_nonaddressable(global_id));

    program.addFormula(rel_mem(global_mem_id));
    program.addFormula(rel_hasAllocatedMemory(global_id, global_mem_id));

    if (global.isConstant()) {
        program.addFormula(rel_immutable(global_mem_id));
    }

    // a few properties to consider
    //   1. constant (immutable)
    //   2. initializer (which may reference other memory objects)
    //   3. external (global var points to any object)

    if (global.hasInitializer()) {
        const Constant *initializer = global.getInitializer();
        unsigned int initializer_id = getObjectIDOfValue(initializer);
        generateFactsForConstant(program, *initializer);

        program.addFormula(rel_hasInitializer(global_id, initializer_id));
    } else {
        program.addFormula(rel_hasNoInitializer(global_id));
    }
}

void FactGenerator::generateFactsForConstant(StandardDatalog::Program &program, const Constant &constant) {
    if (initializedConstants.find(&constant) != initializedConstants.end()) {
        return;
    }

    initializedConstants.insert(&constant);

    generateFactsForValue(program, constant);

    unsigned int constant_id = getObjectIDOfValue(&constant);

    // to be conservative, assume same constants
    // implies same memory location
    program.addFormula(rel_constant(constant_id));
    program.addFormula(rel_immutable(constant_id));
    program.addFormula(rel_nonaddressable(constant_id));

    for (const Use &operand: constant.operands()) {
        auto *constant = dyn_cast<Constant>(operand);
        assert(constant && "non-constant operand of constant value");
        generateFactsForConstant(program, *constant);
    }

    if (auto *aggregate = dyn_cast<ConstantAggregate>(&constant)) {
        // aggregate and all of its fields are alias of each other
        for (const Use &operand: constant.operands()) {
            auto operand_id = getObjectIDOfValue(operand);
            program.addFormula(rel_hasConstantField(constant_id, operand_id));
        }

    } else if (auto *expr = dyn_cast<ConstantExpr>(&constant)) {
        // this is essentially an instruction
        generateFactsForInstruction(program, *expr);
    } else if (auto *global = dyn_cast<GlobalValue>(&constant)) {
        // already handled in other functions
    } else if (auto *data = dyn_cast<ConstantData>(&constant)) {
        // including undef
        // TODO: too conservative here

        if (data->getType()->isPointerTy()) {
            if (isa<UndefValue>(data)) {
                program.addFormula(rel_undef(constant_id));
            } else if (isa<ConstantPointerNull>(data)) {
                // simply use the mem object allocated above
                program.addFormula(rel_null(constant_id));
            }
        } else {
            // assumption: this will never point to anything
            // program.addFormula(rel_nonpointer(constant_id));
        }
    } else {
        // TODO: missing support for basic block address
        assert(0 && "unsupported constant type");
    }
}

/**
 * The most general fact generator for values
 */
void FactGenerator::generateFactsForValue(StandardDatalog::Program &program, const llvm::Value &value) {
    unsigned int val_id = getObjectIDOfValue(&value);
    Type *type = value.getType();

    // whitelist of non-pointer types
    if (type->isIntegerTy() ||
        type->isFloatingPointTy()) {
        // dbgs() << "nonpointer: ";
        // value.print(dbgs());
        // dbgs() << "\n";
        program.addFormula(rel_nonpointer(val_id));
    }
}

/**
 * To add support for a new intrinsics, add a new class here inheriting IntrinsicCall
 */

struct MallocIntrinsicCall: IntrinsicCall {
    virtual MatchResult match(const CallInst *call) override {
        const Function *function = call->getCalledFunction();
    
        if (!function) {
            return { false };
        }

        bool name_match =
            function->getName() == "malloc" ||
            function->getName() == "calloc" ||
            function->getName() == "realloc" ||
            function->getName() == "fopen";

        if (name_match &&
            function->arg_begin()->getType()->isIntegerTy() &&
            function->getReturnType()->isPointerTy()) {
            return { true, 1 };
        }

        return { false };
    }

    virtual void generate(FactGenerator *fact_generator,
                          StandardDatalog::Program &program,
                          const llvm::CallInst *call) override {
        unsigned int instr_id = fact_generator->getObjectIDOfValue(call);
        unsigned int mem_id = fact_generator->getAffiliatedObjectID(instr_id, 1);
        
        program.addFormula(fact_generator->rel_mem(mem_id));
        program.addFormula(fact_generator->rel_intrinsicMalloc(instr_id, mem_id));
    }
};

struct MemcpyIntrinsicCall: IntrinsicCall {
    virtual MatchResult match(const CallInst *call) override {
        const Function *function = call->getCalledFunction();
    
        if (!function) {
            return { false };
        }

        bool name_match =
            function->getName().startswith("llvm.memcpy.") ||
            function->getName().startswith("llvm.memmove.") ||
            function->getName() == "strncpy" ||
            function->getName() == "strcpy";

        if (name_match &&
            function->arg_size() >= 2) {
            return { true, 0 };
        }

        return { false };
    }

    virtual void generate(FactGenerator *fact_generator,
                          StandardDatalog::Program &program,
                          const llvm::CallInst *call) override {
        unsigned int instr_id = fact_generator->getObjectIDOfValue(call);

        assert(call->getNumArgOperands() >= 2);

        unsigned int arg_dest_id = fact_generator->getObjectIDOfValue(call->getArgOperand(0));
        unsigned int arg_src_id = fact_generator->getObjectIDOfValue(call->getArgOperand(1));

        program.addFormula(fact_generator->rel_intrinsicMemcpy(instr_id, arg_dest_id, arg_src_id));
    }
};

/**
 * A collections of calls that we assume to have no side effect
 * to our program
 */
struct ConstantIntrinsicCall: IntrinsicCall {
    const std::set<std::string> callList = {
        "free",
        "printf", "fprintf",
        "__isoc99_scanf", "scanf",
        "fflush", "feof", "_IO_getc",
        "tolower", "fclose", "exit"
    };

    virtual MatchResult match(const CallInst *call) override {
        const Function *function = call->getCalledFunction();
    
        if (function &&
            callList.find(function->getName()) != callList.end()) {
            return { true, 0 };
        }

        return { false };
    }

    virtual void generate(FactGenerator *fact_generator,
                          StandardDatalog::Program &program,
                          const llvm::CallInst *call) override {
        // generate nothing
    }
};

std::vector<IntrinsicCall *> FactGenerator::intrinsicList = {
    new MallocIntrinsicCall(),
    new MemcpyIntrinsicCall(),
    new ConstantIntrinsicCall(),
};
