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
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace std;
using namespace llvm;
using namespace llvm::orc;

// TODO: "static compiler"
/* TODO: implement calls to diagram generator:
    F->viewCFG()
    F->viewCFGOnly()
    where F is a Function*
*/

// ----------------------------------------------------------------------------------------------
// LEXER ========================================================================================
//

// returns [0-255] for unknown characters.
// returns the following for known things.
enum Token
{
    tok_eof = -1,

    // commands
    tok_def = -2,
    tok_extern = -3,

    // primary
    tok_identifier = -4,
    tok_number = -5,

    // control flow
    tok_if = -6,
    tok_then = -7,
    tok_else = -8,
    tok_for = -9,
    tok_in = -10
};

static string IdentifierStr; // used if tok_identifier
static double NumVal;        // used if tok_number----------------------------------------------------------------------------------------------

// gettok - Return next token from std. input
static int gettok()
{
    // TODO: enforce formatting rules here

    static int LastChar = ' ';

    // skip whitespace
    // reads characters one at a time from stdin
    // eats them as it reads them, stores last char red (but not processed) in LastChar
    while (isspace(LastChar))
        LastChar = getchar();

    // if LastChar is a letter, it's part of an identifier
    if (isalpha(LastChar)) // identifier: a-z, A-Z, 0-9
    {
        IdentifierStr = LastChar;

        // get the full identifier
        // isalnum() checks if a char is a decimal digit OR an upper/lowercase letter
        while (isalnum((LastChar = getchar())))
            IdentifierStr += LastChar;

        // if token is "def" or "extern," return those corresponding tokens
        // else return that it is an identifier
        if (IdentifierStr == "def")
            return tok_def;
        if (IdentifierStr == "extern")
            return tok_extern;
        if (IdentifierStr == "if")
            return tok_if;
        if (IdentifierStr == "then")
            return tok_then;
        if (IdentifierStr == "else")
            return tok_else;
        if (IdentifierStr == "for")
            return tok_for;
        if (IdentifierStr == "in")
            return tok_in;
        return tok_identifier;
    }

    // if char is any of the things that make up a double - a number or a decimal, it's a number
    if (isdigit(LastChar) || LastChar == '.')
    { // number: 0-9. +
        string NumStr;

        // TODO: limit to 1 decimal. truncate any that remain.
        do
        {
            NumStr += LastChar;
            LastChar = getchar();

        } while (isdigit(LastChar) || LastChar == '.');

        NumVal = strtod(NumStr.c_str(), nullptr); // convert from str to double, store in NumVal
        return tok_number;                        // return that it is in fact a number
    }

    // comments start with  '?'
    if (LastChar == '?')
    {
        // comment lasts until end of line
        do
        {
            LastChar = getchar();
        } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r'); // all possible EOFs, including the one defined by this language

        if (LastChar != EOF)
            return gettok();
    }

    // all other cases
    // check for end of file. don't eat EOF using getchar. that'd be bad...?
    if (LastChar == EOF)
        return tok_eof;

    // otherwise return char as its ascii value, we dk what else to do with it
    int ThisChar = LastChar;
    LastChar = getchar();
    return ThisChar;
}
// ----------------------------------------------------------------------------------------------
// ABSTRACT SYNTAX TREE =========================================================================
// ----------------------------------------------------------------------------------------------

// base class for all expression nodes

namespace
{
    // each AST class has its own code generation (codegen()) method
    // codegen() emits IR for last AST node and all the things it depends on.
    // returns LLVM Value object -> represents Static Single Assignment (SSA)
    class ExprAST
    {
    public:
        // virtual: lets this destructor be overridden in derived classes
        virtual ~ExprAST() = default;
        virtual Value *codegen() = 0;
    };

    // expression class for numeric literals ie. 1.0
    class NumberExprAST : public ExprAST
    {
        double Val;

    public:
        NumberExprAST(double Val) : Val(Val) {} // constructor that sets value of Val to parameter Val
        Value *codegen() override;
    };

    // expression class for referencing a variable
    class VariableExprAST : public ExprAST
    {
        string Name;

    public:
        VariableExprAST(const string &Name) : Name(Name) {}
        Value *codegen() override;
    };

    // expression class for binary operators
    // aka things that return booleans
    class BinaryExprAST : public ExprAST
    {
        // unique_ptr smart pointer that owns/manages/disposes of its object when outside scope
        char Op;
        unique_ptr<ExprAST> LHS, RHS;

    public:
        BinaryExprAST(char Op, unique_ptr<ExprAST> LHS,
                      unique_ptr<ExprAST> RHS)
            : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}

        Value *codegen() override;
    };

    // expression class for function calls
    class CallExprAST : public ExprAST
    {
        string Callee;
        vector<unique_ptr<ExprAST>> Args;

    public:
        CallExprAST(const string &Callee,
                    vector<unique_ptr<ExprAST>> Args)
            : Callee(Callee), Args(std::move(Args)) {}

        Value *codegen() override;
    };

    // prototype for a function
    // name, arg names (and number of args)
    class PrototypeAST
    {
        string Name;
        vector<string> Args;

    public:
        PrototypeAST(const string &Name, vector<string> Args)
            : Name(Name), Args(std::move(Args)) {}

        Function *codegen();
        const string &getName() const { return Name; }
    };

    // class representing function definition
    class FunctionAST
    {
        unique_ptr<PrototypeAST> Proto;
        unique_ptr<ExprAST> Body;

    public:
        FunctionAST(unique_ptr<PrototypeAST> Proto, unique_ptr<ExprAST> Body)
            : Proto(std::move(Proto)), Body(std::move(Body)) {}

        Function *codegen();
    };

    // expression class AST node for if/then/else -> pointers to subexpressions
    class IfExprAST : public ExprAST
    {
        unique_ptr<ExprAST> Cond, Then, Else;

    public:
        IfExprAST(unique_ptr<ExprAST> Cond, unique_ptr<ExprAST> Then,
                  unique_ptr<ExprAST> Else)
            : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}

        Value *codegen() override;
    };

    class ForExprAST : public ExprAST
    {
        string VarName;
        unique_ptr<ExprAST> Start, End, Step, Body;

    public:
        ForExprAST(const string &VarName, unique_ptr<ExprAST> Start, unique_ptr<ExprAST> End,
                   unique_ptr<ExprAST> Step, unique_ptr<ExprAST> Body)
            : VarName(VarName), Start(std::move(Start)),
              End(std::move(End)), Step(std::move(Step)), Body(std::move(Body)) {}

        Value *codegen() override;
    };
} // end of anonymous namespace

// ----------------------------------------------------------------------------------------------
// PARSER ========================================================================================
// ----------------------------------------------------------------------------------------------

// function stubs
static unique_ptr<ExprAST> ParseExpression();
static unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, unique_ptr<ExprAST> LHS);

// CurTok/getNextToken - provide token buffer around lexer
static int CurTok;
static int getNextToken()
{
    return CurTok = gettok();
}

// error handling helper functions
unique_ptr<ExprAST> LogError(const char *Str)
{
    fprintf(stderr, "Error: %s\n", Str);
    return nullptr;
}

unique_ptr<PrototypeAST> LogErrorP(const char *Str)
{
    LogError(Str);
    return nullptr;
}

// numberexpr ::= number
// called when token is tok_number
// takes current value, makes a NumberExprAST node, advances, returns
static unique_ptr<ExprAST> ParseNumberExpr()
{
    auto Result = make_unique<NumberExprAST>(NumVal); // make a number with the value
    getNextToken();                                   // consume the number
    return std::move(Result);
}

// parenexpr ::= '(' expression ')'
static unique_ptr<ExprAST> ParseParenExpr()
{
    getNextToken(); // eat '('
    auto V = ParseExpression();
    if (!V)
        return nullptr;

    if (CurTok != ')')
        return LogError("expected ')");
    getNextToken(); // eat ')'.
    return V;
}

// ::= identifier
// ::= identifier '(' expression* ')'
// handles variable references and function calls
// called if current token is a tok_identifier token, recursion and error handling
static unique_ptr<ExprAST> ParseIdentifierExpr()
{
    string IdName = IdentifierStr;

    getNextToken(); // eat identifier

    if (CurTok != '(') // simple variable ref 0-> "look ahead" to see that this is a variable.
        // if it is a variable, return and don't do anything fancy
        return make_unique<VariableExprAST>(IdName);

    // call
    getNextToken(); // eat (
    vector<unique_ptr<ExprAST>> Args;
    if (CurTok != ')')
    {
        // continue until it finds a ')'.
        while (true)
        {
            if (auto Arg = ParseExpression())
                Args.push_back(std::move(Arg));
            else
                return nullptr;

            if (CurTok == ')')
                break;

            if (CurTok != ',')
                return LogError("Expected ')' or ',' in argument list.");
            getNextToken(); // eat next token
        }
    }

    getNextToken(); // eat the ')'

    return make_unique<CallExprAST>(IdName, std::move(Args));
}

static unique_ptr<ExprAST> ParseIfExpr()
{
    getNextToken(); // eat the IF

    // condition
    auto Cond = ParseExpression();
    if (!Cond)
        return nullptr;

    if (CurTok != tok_then)
        return LogError("Expected then.");

    getNextToken();                // eat the then
    auto Then = ParseExpression(); // get Then expression
    if (!Then)
        return nullptr;

    if (CurTok != tok_else)
        return LogError("Expected else.");

    getNextToken();
    auto Else = ParseExpression();
    if (!Else)
        return nullptr;

    return make_unique<IfExprAST>(std::move(Cond), std::move(Then), std::move(Else));
}

static unique_ptr<ExprAST> ParseForExpr()
{
    getNextToken(); // eat the for

    if (CurTok != tok_identifier)
        return LogError("Expected identifier after for.");

    string IdName = IdentifierStr;
    getNextToken(); // eat identifier

    if (CurTok != '=')
        return LogError("Expected '=' after for");
    getNextToken(); // eat '='

    auto Start = ParseExpression();
    if (!Start)
        return nullptr;
    if (CurTok != ',')
        return LogError("expected ',' after for's start value");
    getNextToken();

    auto End = ParseExpression();
    if (!End)
        return nullptr;

    // step value is optional -> dont return error if missed
    unique_ptr<ExprAST> Step;
    if (CurTok == ',')
    {
        getNextToken();
        Step = ParseExpression();
        if (!Step)
            return nullptr;
    }

    if (CurTok != tok_in)
        return LogError("Expected 'in' after for");
    getNextToken(); // eat 'in'

    auto Body = ParseExpression();
    if (!Body)
        return nullptr;

    return make_unique<ForExprAST>(IdName, std::move(Start), std::move(End), std::move(Step), std::move(Body));
}

// primary
//  ::= identifierexpr
//  ::= numberexpr
//  ::= parenexpr
// look at an expression that can be any of the 3 above (primary expressions) and decide which one it is
static unique_ptr<ExprAST> ParsePrimary()
{
    switch (CurTok)
    {
    default:
        return LogError("unknown token when expecting expression.");
    case tok_identifier:
        return ParseIdentifierExpr();
    case tok_number:
        return ParseNumberExpr();
    case '(':
        return ParseParenExpr();
    case tok_if:
        return ParseIfExpr();
    case tok_for: // in does not have its own parser, it's part of for
        return ParseForExpr();
    }
}

// operator precendence parsing
// holds precedence for every binary operator defined
static map<char, int> BinopPrecedence;

// get precedence of preceding binary op token
static int GetTokPrecendence()
{
    if (!isascii(CurTok))
        return -1;

    // make sure declared binop
    int TokPrec = BinopPrecedence[CurTok];
    if (TokPrec <= 0)
        return -1;
    return TokPrec;
}

// ::= ('+' primary)*
static unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, unique_ptr<ExprAST> LHS)
{

    // if binop, find its precendence
    while (true)
    {
        int TokPrec = GetTokPrecendence();

        // if this is a binop that binds at least as tightly as the current one consume it,
        // otherwise we are done
        if (TokPrec < ExprPrec)
            return LHS;

        int BinOp = CurTok;
        getNextToken(); // eat binop

        // parse primary exp after binary operator
        auto RHS = ParsePrimary();
        if (!RHS)
            return nullptr;

        // if binop binds less tightly with RHS than the operator after RHS,
        // let the binding operator take RHS as its LHS
        int NextPrec = GetTokPrecendence();
        if (TokPrec < NextPrec)
        {
            RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
            if (!RHS)
                return nullptr;
        }

        // merge LHS/RHS
        LHS = make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));

        // top of while loop again
    }
}

static std::unique_ptr<ExprAST> ParseExpression()
{
    auto LHS = ParsePrimary();
    if (!LHS)
        return nullptr;

    return ParseBinOpRHS(0, std::move(LHS));
}
// prototype
//  ::= id '(' id* ')'
static unique_ptr<PrototypeAST> ParsePrototype()
{
    if (CurTok != tok_identifier)
        return LogErrorP("Expected function name in prototype.");

    string FnName = IdentifierStr;
    getNextToken();

    if (CurTok != '(')
        return LogErrorP("Expected '(' in prototype");

    // read list of arg names
    vector<string> ArgNames;
    while (getNextToken() == tok_identifier)
        ArgNames.push_back(IdentifierStr);

    if (CurTok != ')')
        return LogErrorP("Expected ')' in prototype");

    // success
    getNextToken(); // eat ')'

    return make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

// definition ::= 'def' prototype expression
// function def = prototype wwith expression to implement the body
static unique_ptr<FunctionAST> ParseDefinition()
{
    getNextToken(); // eat def
    auto Proto = ParsePrototype();
    if (!Proto)
        return nullptr;

    if (auto E = ParseExpression())
        return make_unique<FunctionAST>(std::move(Proto), std::move(E));
    return nullptr;
}

// external ::= 'extern' prototype
// prototype with no body
static unique_ptr<PrototypeAST> ParseExtern()
{
    getNextToken(); // eat 'extern'
    return ParsePrototype();
}

// ::= expression
static unique_ptr<FunctionAST> ParseTopLevelExpr()
{
    if (auto E = ParseExpression())
    {
        // anonymous proto
        auto Proto = make_unique<PrototypeAST>("__anon_expr", vector<string>());
        return make_unique<FunctionAST>(std::move(Proto), std::move(E));
    }
    return nullptr;
}

// ----------------------------------------------------------------------------------------------
// ==CODE GENERATION ==============================================================================
// ----------------------------------------------------------------------------------------------

// codegen() emits IR for the AST node and all the things it depends on.
// each thing returns an LLVM Value object. (Value is a Static Single Assignment)

// code generation variables
static unique_ptr<LLVMContext> TheContext; // core LLVM data structures -> type/const value tables
static unique_ptr<IRBuilder<>> Builder;    // helper object - generates LLVM instructions more easily,
                                           // keep track of current place to insert instructions,
                                           // methods to create new ones
static unique_ptr<Module> TheModule;       // LLVM construct. contains functions and global vars -> IR uses this to contain code, owns memory of all IR generated
static map<string, Value *> NamedValues;   // keeps track of values defined in current scope and their LLVM representation is.
                                           // basically a symbol table.
                                           // includes function parameters if applicable.

static unique_ptr<KaleidoscopeJIT> TheJIT;
static unique_ptr<FunctionPassManager> TheFPM;
static unique_ptr<LoopAnalysisManager> TheLAM;
static unique_ptr<FunctionAnalysisManager> TheFAM;
static unique_ptr<CGSCCAnalysisManager> TheCGAM;
static unique_ptr<ModuleAnalysisManager> TheMAM;
static unique_ptr<PassInstrumentationCallbacks> ThePIC;
static unique_ptr<StandardInstrumentations> TheSI;

static map<string, unique_ptr<PrototypeAST>> FunctionProtos;
static ExitOnError ExitOnErr;

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

// code generation for numbers (the only type)
// creates and returns a ConstantFP -> holds APFloat, which holds a float of arbitrary precision.
Value *NumberExprAST::codegen()
{
    return ConstantFP::get(*TheContext, APFloat(Val));
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
    case '+':
        return Builder->CreateFAdd(L, R, "addtmp");
    case '-':
        return Builder->CreateFSub(L, R, "subtmp");
    case '*':
        return Builder->CreateFMul(L, R, "multmp");
    case '<':
        L = Builder->CreateFCmpULT(L, R, "cmptmp");
        // convert bool to double 0.0 or 1.0.
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

// ----------------------------------------------------------------------------------------------
// TOP LEVEL PARSING + JIT Driver ==============================================================
// ----------------------------------------------------------------------------------------------

static void InitializeModuleAndManagers()
{
    // open new context and module
    TheContext = make_unique<LLVMContext>();
    TheModule = make_unique<Module>("KaleidoscopeJIT", *TheContext);
    TheModule->setDataLayout(TheJIT->getDataLayout());

    // create new module builder
    Builder = make_unique<IRBuilder<>>(*TheContext);

    // create new pass and analysis managers
    TheFPM = make_unique<FunctionPassManager>();

    // calculate info to be used by other passes
    // 4 levels of IR hierarchy
    TheLAM = make_unique<LoopAnalysisManager>();
    TheFAM = make_unique<FunctionAnalysisManager>();
    TheCGAM = make_unique<CGSCCAnalysisManager>();
    TheMAM = make_unique<ModuleAnalysisManager>();

    // required for pass instrumentation framework
    // lets devs customize what happens between passes
    ThePIC = make_unique<PassInstrumentationCallbacks>();
    TheSI = make_unique<StandardInstrumentations>(*TheContext, /*DebugLogging*/ true);

    TheSI->registerCallbacks(*ThePIC, TheMAM.get());

    // add transform/optimization passes - actually change IR
    // cleanup operations!
    TheFPM->addPass(InstCombinePass()); // peephole optimizations
    TheFPM->addPass(ReassociatePass()); // reassociate exprs
    TheFPM->addPass(GVNPass());         // eliminate common subexprs
    TheFPM->addPass(SimplifyCFGPass()); // simplify control flow graph (delete unreachable blocks)

    // register analysis passes used by transform passes
    PassBuilder PB;
    PB.registerModuleAnalyses(*TheMAM);
    PB.registerFunctionAnalyses(*TheFAM);
    PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);
}

static void HandleDefinition()
{
    if (auto FnAST = ParseDefinition())
    {
        if (auto *FnIR = FnAST->codegen())
        {
            fprintf(stderr, "Read function definition: ");
            FnIR->print(errs());
            fprintf(stderr, "\n");

            ExitOnErr(TheJIT->addModule(
                ThreadSafeModule(std::move(TheModule), std::move(TheContext))));
            InitializeModuleAndManagers();
        }
    }
    else
    {
        // skip token (error recovery)
        getNextToken();
    }
}

static void HandleExtern()
{
    if (auto ProtoAST = ParseExtern())
    {
        if (auto *FnIR = ProtoAST->codegen())
        {
            fprintf(stderr, "Read extern: ");
            FnIR->print(errs());
            fprintf(stderr, "\n");

            FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
        }
    }
    else
    {
        // error recovery
        getNextToken();
    }
}

// use KaleidoscopeJIT.h to parse top level expressions
// add LLVM IR module to JIT, so its functions are available for execution
// called after parsing and codegen are done
static void HandleTopLevelExpression()
{
    // eval top-level expr into anon function
    if (auto FnAST = ParseTopLevelExpr())
    {
        if (FnAST->codegen())
        {
            // create a ResourceTracker to track JIT'd memory alloc to anon exp
            // this way we can free after exec
            auto RT = TheJIT->getMainJITDylib().createResourceTracker();

            // calling addModule triggers codegen for all functions in module, gets RT
            auto TSM = ThreadSafeModule(std::move(TheModule), std::move(TheContext));
            ExitOnErr(TheJIT->addModule(std::move(TSM), RT));

            // open new module to hold subsequent code
            InitializeModuleAndManagers();

            // search JIT for __anon_expr symbol
            auto ExprSymbol = ExitOnErr(TheJIT->lookup("__anon_expr"));

            // get symbol's address and cast to type
            // call as native function
            double (*FP)() = ExprSymbol.getAddress().toPtr<double (*)()>();
            fprintf(stderr, "Evaluated to %f\n", FP());

            // delete anon expr module from JIT -> no re-eval
            ExitOnErr(RT->remove());
        }
    }
    else
    {
        getNextToken();
    }
}

// definition | external | expression | ';'
static void MainLoop()
{
    while (true)
    {
        fprintf(stderr, "ready> \n");
        switch (CurTok)
        {
        case tok_eof:
            return;
        case ';': // ignore top-level semicolons - top-level expression may not have one
            getNextToken();
            break;
        case tok_def:
            HandleDefinition();
            break;
        case tok_extern:
            HandleExtern();
            break;
        default:
            // fprintf(stderr, "Entering HandleTopLevelExpression()");
            HandleTopLevelExpression();
            break;
        }
    }
}

// ----------------------------------------------------------------------------------------------
// == LIBRARY FUNCTIONS ========================================================================
// ----------------------------------------------------------------------------------------------

// on windows, export the functions because dynamic symbol loader will use
// GetProcAddress to find symbols
#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

// PRODUCE CONSOLE OUTPUT USING C ----

// putchard - putchar takes a double ASCII value and returns 0
// writes a single char from ASCII to stdout
extern "C" DLLEXPORT double putchard(double X)
{
    fputc((char)X, stderr);
    return 0;
}

// printd - printf that takes a double and prints with new line, returns 0
extern "C" DLLEXPORT double printd(double X)
{
    fprintf(stderr, "%f\n", X);
    return 0;
}

// ----------------------------------------------------------------------------------------------
// ==DRIVER CODE ===================================================================================
// ----------------------------------------------------------------------------------------------

int main()
{
    // prepare environment and initialize JIT
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    // install std binary ops
    // 1 is lowest precedence
    BinopPrecedence['<'] = 10;
    BinopPrecedence['+'] = 20;
    BinopPrecedence['-'] = 20;
    BinopPrecedence['*'] = 40; // highest

    // prime first token
    fprintf(stderr, "ready>\n");
    getNextToken();

    TheJIT = ExitOnErr(KaleidoscopeJIT::Create());

    // make a module to hold the code
    InitializeModuleAndManagers();

    // run main loop
    MainLoop();

    // print out generated code
    TheModule->print(errs(), nullptr);

    return 0;
}
