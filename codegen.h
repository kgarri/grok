#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "KaleidoscopeJIT.h"

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"

#include <memory>
#include <map>

using namespace std;
using namespace llvm;
using namespace llvm::orc;

// code generation variables
extern unique_ptr<LLVMContext> TheContext; // core LLVM data structures -> type/const value tables
extern unique_ptr<IRBuilder<>> Builder;    // helper object - generates LLVM instructions more easily,
                                           // keep track of current place to insert instructions,
                                           // methods to create new ones
extern unique_ptr<Module> TheModule;       // LLVM construct. contains functions and global vars -> IR uses this to contain code, owns memory of all IR generated
extern map<string, Value *> NamedValues;   // keeps track of values defined in current scope and their LLVM representation is.
                                           // basically a symbol table.
                                           // includes function parameters if applicable.

extern unique_ptr<KaleidoscopeJIT> TheJIT;
extern unique_ptr<FunctionPassManager> TheFPM;
extern unique_ptr<LoopAnalysisManager> TheLAM;
extern unique_ptr<FunctionAnalysisManager> TheFAM;
extern unique_ptr<CGSCCAnalysisManager> TheCGAM;
extern unique_ptr<ModuleAnalysisManager> TheMAM;
extern unique_ptr<PassInstrumentationCallbacks> ThePIC;
extern unique_ptr<StandardInstrumentations> TheSI;

extern map<string, unique_ptr<PrototypeAST>> FunctionProtos;
extern ExitOnError ExitOnErr;

Value *LogErrorV(const char *Str);
Function *getFunction(string Name);

#endif
