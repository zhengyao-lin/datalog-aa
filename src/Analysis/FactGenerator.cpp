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

    // constant may have operands in case of
    // constant aggregate or constant expression
    for (const Use &operand: constant.operands()) {
        auto *constant_operand = dyn_cast<Constant>(operand);
        assert(constant_operand && "non-constant operand of constant value");
        initObjectIDForConstant(*constant_operand);
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
    unsigned int function_id = getObjectIDOfValue(&function);
    unsigned int function_mem_id = getAffiliatedObjectID(function_id, 1);

    program.addFormula(rel_function(function_id));
    program.addFormula(rel_mem(function_mem_id));
    program.addFormula(rel_pointsTo(function_id, function_mem_id));

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
    unsigned int block_id = getObjectIDOfValue(&block);
    unsigned int function_id = getObjectIDOfValue(block.getParent());

    program.addFormula(rel_block(block_id));
    program.addFormula(rel_immutable(block_id));
    program.addFormula(rel_hasBlock(function_id, block_id));

    for (const Instruction &instr: block) {
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
            const Value *value = user.getOperand(0);
            unsigned int value_id = getObjectIDOfValue(value);
            program.addFormula(rel_instrRet(instr_id, value_id));
            break;
        }

        case Instruction::BitCast: {
            const Value *value = user.getOperand(0);
            unsigned int value_id = getObjectIDOfValue(value);
            program.addFormula(rel_instrBitCast(instr_id, value_id));
            break;
        }

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

        // this way of checking if a function has definition or not
        // comes from https://github.com/grievejia/andersen/blob/master/lib/ConstraintCollect.cpp#L351
        case Instruction::Call: {
            const CallInst *call = dyn_cast<CallInst>(&user);
            assert(call && "not a call instruction");

            const Function *function = call->getCalledFunction();
            
            unsigned int call_id = getObjectIDOfValue(call);
            unsigned int function_id = getObjectIDOfValue(function);

            if (function->isDeclaration() || function->isIntrinsic()) {
                // TODO: handle external library calls and intrinsic calls
                // e.g. malloc, realloc, etc.
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
        };

        default:
        UNKNOWN_INSTR:
            program.addFormula(rel_instrUnknown(instr_id));
            dbgs() << "unsupported instruction ";
            user.print(dbgs());
            dbgs() << "\n";
    }
}

void FactGenerator::generateFactsForGlobalVariable(StandardDatalog::Program &program, const GlobalVariable &global) {
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
    program.addFormula(rel_pointsTo(global_id, global_mem_id));

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

    unsigned int constant_id = getObjectIDOfValue(&constant);

    // to be conservative, assume same constants
    // implies same memory location
    program.addFormula(rel_constant(constant_id));
    program.addFormula(rel_immutable(constant_id));

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
        }
    } else {
        // TODO: missing support for basic block address
        assert(0 && "unsupported constant type");
    }
}
