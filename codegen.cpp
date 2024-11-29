#include "codegen.h"
#include "parser.h"
#include "ast.h"
#include "lexer.h"

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

// ----------------------------------------------------------------------------------------------
// ==CODE GENERATION ==============================================================================
// ----------------------------------------------------------------------------------------------

// codegen() emits IR for the AST node and all the things it depends on.
// each thing returns an LLVM Value object. (Value is a Static Single Assignment)

// code generation variables
unique_ptr<LLVMContext> TheContext; // core LLVM data structures -> type/const value tables
unique_ptr<IRBuilder<>> Builder;    // helper object - generates LLVM instructions more easily,
                                    // keep track of current place to insert instructions,
                                    // methods to create new ones
unique_ptr<Module> TheModule;       // LLVM construct. contains functions and global vars -> IR uses this to contain code, owns memory of all IR generated
map<string, Value *> NamedValues;   // keeps track of values defined in current scope and their LLVM representation is.
                                    // basically a symbol table.
                                    // includes function parameters if applicable.

unique_ptr<KaleidoscopeJIT> TheJIT;
unique_ptr<FunctionPassManager> TheFPM;
unique_ptr<LoopAnalysisManager> TheLAM;
unique_ptr<FunctionAnalysisManager> TheFAM;
unique_ptr<CGSCCAnalysisManager> TheCGAM;
unique_ptr<ModuleAnalysisManager> TheMAM;
unique_ptr<PassInstrumentationCallbacks> ThePIC;
unique_ptr<StandardInstrumentations> TheSI;

map<string, unique_ptr<PrototypeAST>> FunctionProtos;
ExitOnError ExitOnErr;

// reports errors found during code generation
Value *LogErrorV(const char *Str)
{
    LogError(Str);
    return nullptr;
}

// convenience method, just calls TheModule->getFunction() if it finds an existing function def
// if not, try to generate one or else return null
Function *getFunction(string Name)
{
    // FunctionProtos holds most recent prototype for each function
    //  see if function has been already added to the current module
    if (auto *F = TheModule->getFunction(Name))
        return F;

    // if not, check whether we can codegen declaration from prototype
    auto FI = FunctionProtos.find(Name);
    if (FI != FunctionProtos.end())
        return FI->second->codegen();

    // no prototype exists, return null
    return nullptr;
}

// code generation for numbers
// creates and returns a ConstantFP -> holds APFloat, which holds a float of arbitrary precision.
Value *NumberExprAST::codegen()
{
    return ConstantFP::get(*TheContext, APFloat(Val));
}

/*
    https://llvm.org/doxygen/IRBuilder_8h_source.html (line 453)
  /// Make a new global variable with an initializer that has array of i8 type
  /// filled in with the null terminated string value specified.  The new global
  /// variable will be marked mergable with any others of the same contents.  If
  /// Name is specified, it is the name of the global variable created.
  ///
  /// If no module is given via \p M, it is take from the insertion point basic
  /// block.
  GlobalVariable *CreateGlobalString(StringRef Str, const Twine &Name = "",
                                     unsigned AddressSpace = 0,
                                     Module *M = nullptr, bool AddNull = true);
*/

// TODO: codegen for strings!!
Value *StringExprAST::codegen()
{
    // fprintf(stderr, "Parsed a string.");
    return Builder->CreateGlobalString(StringRef(StrVal), "");
}

// codegen for variables
Value *VariableExprAST::codegen()
{
    // look up var in the function
    Value *V = NamedValues[Name];
    if (!V)
        return LogErrorV("Unknown variable name.");
    return V;
}

// code generation for binary expressions
Value *BinaryExprAST::codegen()
{
    // L and R must have the same type
    // resulting type must match as well.

    Value *L = LHS->codegen();
    Value *R = RHS->codegen();
    if (!L || !R)
        return nullptr;

    // reminder: Builder helps generate LLVM instructions
    // this includes keeping track of where to insert them, and creating new ones.
    // all it needs are the L and R operands, and what instruction to create!
    // if there are multiple of the same type of expression, each one gets its own unique numeric suffix.
    switch (Op)
    {
        // all of the following IRBuilder functions are defined in IRBuilder.h <3
        // string params are Twine names passed to IRBuilder
    case '+':
        return Builder->CreateFAdd(L, R, "addtmp");
    case '-':
        return Builder->CreateFSub(L, R, "subtmp");
    case '*':
        return Builder->CreateFMul(L, R, "multmp");
    case '/':
        return Builder->CreateFDiv(L, R, "divtmp");
    case '%':
        return Builder->CreateFRem(L, R, "remtmp");
    case '<':
        L = Builder->CreateFCmpULT(L, R, "cmptmp");
        // convert bool to double 0.0 or 1.0.
        return Builder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp");
    case '>':
        L = Builder->CreateFCmpUGT(L, R, "cmptmp");
        return Builder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp");
    default:
        return LogErrorV("invalid binary operator: ");
    }
}

// code generation for functions
Value *CallExprAST::codegen()
{
    // look up name in global module table
    Function *CalleeF = getFunction(Callee);
    if (!CalleeF)
        return LogErrorV("Unknown function referenced: ");

    // if arg mismatch error
    if (CalleeF->arg_size() != Args.size())
        return LogErrorV("Incorrect # arguments passed.");

    // recursively call codegen() for each arg passed, and create an LLVM call instr
    // also allows us to call standard C functions, like sin and cos!
    std::vector<Value *> ArgsV;
    for (unsigned i = 0, e = Args.size(); i != e; ++i)
    {
        ArgsV.push_back(Args[i]->codegen());
        if (!ArgsV.back())
            return nullptr;
    }

    return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

Value *IfExprAST::codegen()
{
    Value *CondV = Cond->codegen();
    if (!CondV)
        return nullptr;
    // convert condition to bool by comparing non-eq to 0.0
    // emit expression for condition, compare that value to 0 to get truth value as a 1 or 0
    CondV = Builder->CreateFCmpONE(CondV, ConstantFP::get(*TheContext, APFloat(0.0)), "ifcond");

    Function *TheFunction = Builder->GetInsertBlock()->getParent(); // parent of current block is the function it goes into

    // create blocks for then and else, insert "then" block at end of function
    BasicBlock *ThenBB = BasicBlock::Create(*TheContext, "then", TheFunction);
    BasicBlock *ElseBB = BasicBlock::Create(*TheContext, "else");
    BasicBlock *MergeBB = BasicBlock::Create(*TheContext, "ifcont");

    Builder->CreateCondBr(CondV, ThenBB, ElseBB);

    // emit then value
    Builder->SetInsertPoint(ThenBB);

    Value *ThenV = Then->codegen();
    if (!ThenV)
        return nullptr;

    Builder->CreateBr(MergeBB);

    // codegen of Then can change the current block and update Then
    // may have changed since we last called this (call it again)
    ThenBB = Builder->GetInsertBlock();

    // emit else block
    TheFunction->insert(TheFunction->end(), ElseBB);
    Builder->SetInsertPoint(ElseBB);

    Value *ElseV = Else->codegen();
    if (!ElseV)
        return nullptr;

    Builder->CreateBr(MergeBB);

    // codegen of else can change current block, update Else for PHI
    ElseBB = Builder->GetInsertBlock(); // add merge block to function object

    // emit merge block
    TheFunction->insert(TheFunction->end(), MergeBB); // changes insertion point so new code goes into merge block
    Builder->SetInsertPoint(MergeBB);
    PHINode *PN = Builder->CreatePHI(Type::getDoubleTy(*TheContext), 2, "iftmp");

    PN->addIncoming(ThenV, ThenBB);
    PN->addIncoming(ElseV, ElseBB);
    return PN;
}

Value *ForExprAST::codegen()
{
    // emit start code first without 'variable' (starting value) in scope
    Value *StartVal = Start->codegen();
    if (!StartVal)
        return nullptr;

    // set up llvm basic block for loop body -> may be multiple blocks
    // make new basic block for loop header, inserting after current block
    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    BasicBlock *PreheaderBB = Builder->GetInsertBlock();
    BasicBlock *LoopBB = BasicBlock::Create(*TheContext, "loop", TheFunction);

    // insert explicit fall through from current block to LoopBB
    Builder->CreateBr(LoopBB);

    // create actual block that starts loop and create unconditional branch for fallthrough between blocks
    // start insertion in LoopBB
    Builder->SetInsertPoint(LoopBB);

    // start PHI node with entry for start
    PHINode *Variable = Builder->CreatePHI(Type::getDoubleTy(*TheContext), 2, VarName);
    Variable->addIncoming(StartVal, PreheaderBB);

    // emit code for loop body
    // save variable that is defined equal to PHI node, restore later
    // allows variable shadowing!!
    Value *OldVal = NamedValues[VarName];
    NamedValues[VarName] = Variable;

    // emit body - ignore value and dont allow errors (check if it exists)
    if (!Body->codegen())
        return nullptr;

    // codegen the body
    // emit step value
    Value *StepVal = nullptr;
    if (Step)
    {
        StepVal = Step->codegen();
        if (!StepVal)
            return nullptr;
    }
    else
    {
        // if not specified, use 1.o
        StepVal = ConstantFP::get(*TheContext, APFloat(1.0));
    }

    Value *NextVar = Builder->CreateFAdd(Variable, StepVal, "nextvar");
    Value *EndCond = End->codegen();
    if (!EndCond)
        return nullptr;

    // convert condition to bool by comparing non-eq to 0.0
    EndCond = Builder->CreateFCmpONE(
        EndCond, ConstantFP::get(*TheContext, APFloat(0.0)), "loopcond");

    // eval exit value of loop to determine if exit - like if/then/else
    // create after loop block and insert
    BasicBlock *LoopEndBB = Builder->GetInsertBlock();
    BasicBlock *AfterBB = BasicBlock::Create(*TheContext, "afterloop", TheFunction);

    // insert conditional branc into the end of LoopEndBB
    Builder->CreateCondBr(EndCond, LoopBB, AfterBB);

    // set insertion point to AfterBB
    Builder->SetInsertPoint(AfterBB);

    // CLEANUPS ----
    // add new entry to PHI node for backedge
    Variable->addIncoming(NextVar, LoopEndBB);

    // restore unshadowed variable
    if (OldVal)
        NamedValues[VarName] = OldVal;
    else
        NamedValues.erase(VarName);

    return Constant::getNullValue(Type::getDoubleTy(*TheContext));
}

// codegen() for functions
// creates the function prototype but not body
// works for extern stmts but not functions ('defined in another source file')
Function *PrototypeAST::codegen()
{
    // function type: double(double, double)
    vector<Type *> Doubles(Args.size(), Type::getDoubleTy(*TheContext));
    FunctionType *FT = FunctionType::get(Type::getDoubleTy(*TheContext), Doubles, false); // creates FunctionType like "new"

    // external linkage means function may be defined outside current module, or callable by functions outside module
    // name is user-specified function name, used in symbol table
    Function *F = Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get()); // creates IR function for the prototype

    // set names for args
    unsigned Idx = 0;
    for (auto &Arg : F->args())
        Arg.setName(Args[Idx++]);

    return F;
}

// generates function with body
Function *FunctionAST::codegen()
{
    // transfer ownership of prototype to FunctionProtos map
    // but keep reference for later
    auto &P = *Proto;
    FunctionProtos[Proto->getName()] = std::move(Proto);
    Function *TheFunction = getFunction(P.getName());
    if (!TheFunction)
        return nullptr;

    // create new basic block to insert into
    // basic blocks define control flow graph
    BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", TheFunction);
    Builder->SetInsertPoint(BB);

    // record function args in NamedValues map
    NamedValues.clear();
    for (auto &Arg : TheFunction->args())
        NamedValues[string(Arg.getName())] = &Arg;

    // add function args to NamedValues map, so they're accessible to VariableExprAST nodes
    if (Value *RetVal = Body->codegen()) // use codegen() to create and store code from entry block
    {
        // finish function
        Builder->CreateRet(RetVal);

        // validate generated code, check for consistency
        verifyFunction(*TheFunction); // provided by LLVM: consistency checks for compiler

        // optimize the function
        TheFPM->run(*TheFunction, *TheFAM);

        return TheFunction;
    }

    // error reading body, remove function -> allows user to retype if they fuck up
    TheFunction->eraseFromParent();
    return nullptr;

    // TODO: Bug!
    //  if FunctionAST::codegen() finds an existing IR function, it doesn't validate the sig against the def's prototype
    //  earlier extern declaration will take precedence over def sig, so codegen may fail
    //  ie. if extern and function args are named differently.
    /*
    extern foo(a);
    def foo(b) b;
    */
}
