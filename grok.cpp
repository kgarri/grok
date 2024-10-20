#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
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
#include "GrokJIT.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <map>
#include <memory>
#include <string>
#include <utility> 
#include <vector>

using namespace llvm;
using namespace llvm::orc;

enum Token{
    //end of file
    tok_eof = -1,

    //commands
    tok_def = -2, //function defs 
    tok_extern = -3,

    //primary
    tok_identifier = -4, //identifiers are whatever the programmer writes
    tok_number = -5 
};

static std::string IdentifierStr; //Filled in if tok_identifier 
static double NumVal; //Filled in if tok_number

static int gettok(){
    static int LastChar = ' ';

    //skipping any whitespace
    while (isspace(LastChar)) 
        LastChar = getchar();

    if (isalpha(LastChar)) {
        IdentifierStr = LastChar;
        while (isalnum((LastChar = getchar())))
            IdentifierStr += LastChar; 

        if (IdentifierStr == "def")
            return tok_def;
        if (IdentifierStr == "extern") 
            return tok_extern;
        return tok_identifier;
    }

    if (isdigit(LastChar) || LastChar =='.'){
        std::string NumStr;
        do {
            NumStr +=LastChar;
            LastChar = getchar(); 
        } while(isdigit(LastChar) || LastChar =='.');

        NumVal = strtod(NumStr.c_str(), 0);
        return tok_number;
    }

    if (LastChar == '#'){
        //comment until end of line
        do  
        LastChar = getchar();
        while(LastChar != EOF && LastChar != '\n' && LastChar != '\r');

        if (LastChar !=EOF) 
            return gettok();
    }
    if (LastChar == EOF) 
        return tok_eof;

    int ThisChar = LastChar;
    LastChar = getchar();
    return ThisChar;
}



//-----------------------------------
//Start of Abstract Syntax Tree
//-----------------------------------
//ExprAST - Base classfor all expression node . 
class ExprAST{
public:
    virtual ~ExprAST() = default;
    virtual Value *codegen() = 0;
};

//NumberExprAST - Expression class for numeric literals like "1.0". 
class NumberExprAST : public ExprAST {
    double Val;
public: 
    NumberExprAST(double Val) : Val(Val) {}
    Value *codegen() override;
};

//VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST {
    std::string Name;
public: 
    VariableExprAST(const std::string &Name) : Name(Name){}
Value *codegen() override;
};

// BinaryExprAST - Expression class for a binary operator
class BinaryExprAST : public ExprAST {
    char Op;
    std::unique_ptr<ExprAST> LHS, RHS;

public: 
    BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                  std::unique_ptr<ExprAST> RHS)
    : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
Value *codegen() override;
};

//CallExprAS - Expression class for function calls . 
class CallExprAST : public ExprAST {
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;
public: 
    CallExprAST(const std::string &Callee, 
                std::vector<std::unique_ptr<ExprAST>> Args)
    :Callee(Callee), Args(std::move(Args)) {}
Value *codegen() override;
};

//PrototypeAST - This class repersents the "prototype"  for a functiopn,
// which captures its name, and its argument names (thus implicitly the number 
// of arguments the function)

class PrototypeAST {
    std::string Name;
    std::vector<std::string> Args;
public:
    PrototypeAST(const std::string &Name, std::vector<std::string> Args)
    : Name(Name), Args(std::move(Args)) {}
    const std::string &getName() const {return Name;}
    Function *codegen();
};

//FunctionAST - This class represents a function definition itself.
class FunctionAST { 
    std::unique_ptr<PrototypeAST> Proto;
    std::unique_ptr<ExprAST> Body;
public:
    FunctionAST(std::unique_ptr<PrototypeAST> Proto,
                std::unique_ptr<ExprAST> Body)
    :Proto(std::move(Proto)), Body(std::move(Body)) {}
    Function *codegen();
};

//-----------------------------------
//End of Abstract Syntax Tree
//-----------------------------------






//BinopPrecendence- This holds the precedence for each binary operator that is 
//defined
//Parser Variables used to identify the tokens and keep object precedence
static std::map<char,int> BinopPrecedence;
static int CurTok;

//Codegeneration variables
static std::unique_ptr<LLVMContext> TheContext;
static std::unique_ptr<IRBuilder<>> Builder; 
static std::unique_ptr<Module> TheModule;
static std::map<std::string, Value *> NamedValues;

// Error checking functions 
std::unique_ptr<ExprAST> LogError(const char *Str);
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str);
Value *LogErrorV(const char *Str);

// These are the parser functions used to build up the tokens for the lanuagag
static std::unique_ptr<ExprAST> ParseNumberExpr();
static std::unique_ptr<ExprAST> ParseParenExpr();
static std::unique_ptr<ExprAST> ParseIdentifierExpr();
static std::unique_ptr<ExprAST> ParsePrimary();
static int GetTokPrecendence();
static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, 
                                              std::unique_ptr<ExprAST> LHS);
static std::unique_ptr<ExprAST> ParseExpression();
static std::unique_ptr<PrototypeAST> ParsePrototype();
static std::unique_ptr<FunctionAST> ParseDefinition();
static std::unique_ptr<PrototypeAST> ParseExtern();
static int getNextToken();
static std::unique_ptr<FunctionAST> ParseTopLevelExpr();

static std::unique_ptr<GrokJIT> TheJIT;
static std::unique_ptr<FunctionPassManager> TheFPM;
static std::unique_ptr<LoopAnalysisManager> TheLAM;
static std::unique_ptr<FunctionAnalysisManager> TheFAM;
static std::unique_ptr<CGSCCAnalysisManager> TheCGAM;
static std::unique_ptr<ModuleAnalysisManager> TheMAM;
static std::unique_ptr<PassInstrumentationCallbacks> ThePIC;
static std::unique_ptr<StandardInstrumentations> TheSI;
static std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
static ExitOnError ExitOnErr;

static void InitializeModuleAndPassManager();
static void HandleDefinition();
static void HandleExtern();
static void HandleTopLevelExpression();
static void MainLoop();

int main() {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    BinopPrecedence['<'] = 10;
    BinopPrecedence['+'] = 20;
    BinopPrecedence['-'] = 20;
    BinopPrecedence['*'] = 40;
    BinopPrecedence['/'] = 40;

    fprintf(stderr, "ready> ");
    getNextToken();

    InitializeModuleAndPassManager();
    
    TheJIT = ExitOnErr(GrokJIT::Create());

    MainLoop();

    TheModule->print(errs(),nullptr);
    return 0;
}

//-----------------------------------
//Start of Parser
//-----------------------------------

//CurTok/getNextToken - Provide a simple token buffer. CurTok is the current
// toke the parser is looking at. getNextToken reads another token from the 
// lexer and updates  CurTok with its results. 

static int getNextToken(){
    return CurTok = gettok();
}

//LogError* - These are little helper functions for error handlingi
std::unique_ptr<ExprAST> LogError(const char *Str){
    fprintf(stderr, "Error %s\n",Str);
    return nullptr;
}
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str){
    LogError(Str);
    return nullptr;
}
Value *LogErrorV(const char *Str){
    LogError(Str);
    return nullptr;
}

// numberexpr ::= number
static std::unique_ptr<ExprAST> ParseNumberExpr() {
    auto Result = std::make_unique<NumberExprAST>(NumVal);
    getNextToken();
    return std::move(Result);
}

//parenexpr ::= '('expression')'
static std::unique_ptr<ExprAST> ParseParenExpr() {
    getNextToken();
    auto v = ParseExpression();
    if (!v)
        return nullptr;
    if (CurTok != ')' )
        return LogError("expected ')'");
    getNextToken();
    return v;
}

//identifierexpr
//::= identifier 
//::= identifier '(' expression')'
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
    std::string IdName = IdentifierStr;
    getNextToken();

    if (CurTok != '(' )
        return std::make_unique<VariableExprAST>(IdName);

    getNextToken();
    std::vector<std::unique_ptr<ExprAST>> Args;
    if (CurTok != ')') {
        while (true){
            if (auto Arg = ParseExpression())
                Args.push_back(std::move(Arg));
            else 
                return nullptr;

            if (CurTok == ')')
                break;

            if (CurTok != ',')
                return LogError("Expected ')' or ',' in argument list");
            getNextToken();
        }
    }
    getNextToken();

    return std::make_unique<CallExprAST>(IdName, std::move(Args));
}

//primary 
// ::=identifierexpr
// ::=numberexpr
// ::=parenexpr
static std::unique_ptr<ExprAST> ParsePrimary() {
    switch (CurTok){
        default:
            return LogError("unkown token when expecting an expression");
        case tok_identifier:
            return ParseIdentifierExpr();
        case tok_number:
            return ParseNumberExpr();
        case '(':
            return ParseParenExpr();
    }
}



//GetTokPrecendence - Get the precedence of the pending binary operator token
static int GetTokPrecendence() {
    if(!isascii(CurTok))
        return -1;

    int TokPrec = BinopPrecedence[CurTok];
    if (TokPrec <=0 )return -1;
    return TokPrec;

}

static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, 
                                              std::unique_ptr<ExprAST> LHS){
    while(true) {
        int TokPrec = GetTokPrecendence();

        if (TokPrec < ExprPrec)
            return LHS;
        int BinOp = CurTok; 
        getNextToken();

        auto RHS = ParsePrimary();
        if (!RHS)
            return nullptr;

        int NextPrec = GetTokPrecendence();
        if (TokPrec < NextPrec) {
            RHS = ParseBinOpRHS(TokPrec+1, std::move(RHS));
            if (!RHS)
                return nullptr;
        }

        LHS = std::make_unique<BinaryExprAST> (BinOp, std::move(LHS),
                                               std::move(RHS));
    }

}

static std::unique_ptr<ExprAST> ParseExpression(){
    auto LHS = ParsePrimary();
    if (!LHS)
        return nullptr;
    return ParseBinOpRHS(0, std::move(LHS));
}
/// prototype
///   ::= id '(' id* ')'
static std::unique_ptr<PrototypeAST> ParsePrototype() {
  if (CurTok != tok_identifier)
    return LogErrorP("Expected function name in prototype");

  std::string FnName = IdentifierStr;
  getNextToken();
  if (CurTok != '(')
    return LogErrorP("Expected '(' in prototype");

  // Read the list of argument names.
  std::vector<std::string> ArgNames;
  while (getNextToken() == tok_identifier)
    ArgNames.push_back(IdentifierStr);
  if (CurTok != ')')
    return LogErrorP("Expected ')' in prototype");

  // success.
  getNextToken();  // eat ')'.

  return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

/// definition ::= 'def' prototype expression
static std::unique_ptr<FunctionAST> ParseDefinition() {
  getNextToken();  // eat def.
  auto Proto = ParsePrototype();
  if (!Proto) return nullptr;

  if (auto E = ParseExpression())
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  return nullptr;
}

/// external ::= 'extern' prototype
static std::unique_ptr<PrototypeAST> ParseExtern() {
  getNextToken();  // eat extern.
  return ParsePrototype();
}

/// toplevelexpr ::= expression
static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
  if (auto E = ParseExpression()) {
    // Make an anonymous proto.
    auto Proto = std::make_unique<PrototypeAST>("", std::vector<std::string>());
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  }
  return nullptr;
}

static void InitializeModuleAndPassManager() {
    // Open a new context and module.
    TheContext = std::make_unique<LLVMContext>();
    TheModule = std::make_unique<Module>("GrokJIT", *TheContext);
    TheModule -> setDataLayout(TheJIT->getDataLayout());

    // Create a new builder for the module.
    Builder = std::make_unique<IRBuilder<>>(*TheContext);

    //Create new pass and analysis managers.
    TheFPM = std::make_unique<FunctionPassManager>();
    TheLAM = std::make_unique<LoopAnalysisManager>();
    TheFAM = std::make_unique<FunctionAnalysisManager>();
    TheCGAM = std::make_unique<CGSCCAnalysisManager>();
    TheMAM = std::make_unique<ModuleAnalysisManager>();
    ThePIC = std::make_unique<PassInstrumentationCallbacks>();
    TheSI = std::make_unique<StandardInstrumentations>(*TheContext, 
                                                       true);
    TheSI->registerCallbacks(*ThePIC, TheMAM.get());

    //Add Transform passes
    //Do simple "peephole" optimizations and bit-twiddling optzns
    TheFPM->addPass(InstCombinePass());
    //Reassociate expressions.
    TheFPM->addPass(ReassociatePass());
    //Eliminate Common SubExpressions.
    TheFPM ->addPass(GVNPass());
    //Simplifiy the control flow graph (deleting unreachable blocks, etc).
    TheFPM->addPass(SimplifyCFGPass());

    //Register analysis passes used in these transform passes 
    PassBuilder PB;
    PB.registerModuleAnalyses(*TheMAM);
    PB.registerFunctionAnalyses(*TheFAM);
    PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);

}

static void HandleDefinition() {
  if (auto FnAST = ParseDefinition()) {
    if (auto *FnIR = FnAST->codegen()) {
      fprintf(stderr, "Read function definition:");
      FnIR->print(errs());
      fprintf(stderr, "\n");
      ExitOnErr(TheJIT->addModule(
                ThreadSafeModule(std::move(TheModule), std::move(TheContext))));
      InitializeModuleAndPassManager();
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleExtern() {
  if (auto ProtoAST = ParseExtern()) {
    if (auto *FnIR = ProtoAST->codegen()) {
      fprintf(stderr, "Read extern: ");
      FnIR->print(errs());
      fprintf(stderr, "\n");
      FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (auto FnAST = ParseTopLevelExpr()) {
    if (FnAST->codegen()) {
        //Create a ResourceTracker to track JIT'd memory allocated to 
        //our anonymous expression -- that way we can free it after executing
        auto RT = TheJIT->getMainJITDylib().createResourceTracker();

        auto TSM = ThreadSafeModule(std::move(TheModule), std::move(TheContext));

        ExitOnErr(TheJIT->addModule(std::move(TSM), RT));

        InitializeModuleAndPassManager();

        // Search the JIT for the __ano_expr symbol
        auto ExprSymbol = ExitOnErr(TheJIT->lookup("__anon_expr"));

        //Get the symbol's adress and cast it to the right type (takes no
        // arguments, return s a double) so we can call it as a native function.
        double (*FP)() = ExprSymbol.getAddress().toPtr<double (*)()>();
        fprintf(stderr, "Evaluated to %f\n", FP());
        
        //Delete the anonymous expression module from JIT
        ExitOnErr(RT->remove());
        }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

/// top ::= definition | external | expression | ';'
static void MainLoop() {
  while (true) {
    fprintf(stderr, "ready> ");
    switch (CurTok) {
    case tok_eof:
      return;
    case ';': // ignore top-level semicolons.
      getNextToken();
      break;
    case tok_def:
      HandleDefinition();
      break;
    case tok_extern:
      HandleExtern();
      break;
    default:
      HandleTopLevelExpression();
      break;
    }
  }
}


Function *getFunction(std::string Name) {
    //First, see if the function has already been added to the current module. 
    if (auto *F = TheModule->getFunction(Name))
        return F;

    //If not, check whether we can codegen the devclarration from some existing
    //prototype
    auto FI = FunctionProtos.find(Name);
    if(FI != FunctionProtos.end())
        return FI->second->codegen();

    return nullptr;
}

//-----------------------------------
//Start of Codegeneration functions
//-----------------------------------

Value *NumberExprAST::codegen(){
    return ConstantFP::get(*TheContext, APFloat(Val));
}

Value *VariableExprAST::codegen(){
    Value *V = NamedValues[Name];
    if (!V)
        LogErrorV("Unkown variable name");
    return V;
}

Value *BinaryExprAST::codegen(){
    Value *L = LHS->codegen();
    Value *R = RHS->codegen();
    if(!L || !R)
        return nullptr;

    switch (Op){
        case'+':
            return Builder->CreateFAdd(L,R,"addtmp");
        case'-':
            return Builder->CreateFSub(L,R,"subtmp");
        case'*':
            return Builder->CreateFMul(L,R,"multmp");
        case'/':
            return Builder->CreateFDiv(L, R,"divtmp");
        case'<':
            L = Builder->CreateFCmpULT(L,R,"cmptmp");
            //Convert Bool 0/1 to double 0.0 or 1.0
            return Builder-> CreateUIToFP(L, Type::getDoubleTy(*TheContext),
                                          "booltmp");
        default:
            return LogErrorV("invaliad binary operator");
    }
}

Value *CallExprAST::codegen() {
    //Look up the name in the global module table.
    Function *CalleeF =  getFunction(Callee);
    if(!CalleeF)
        return LogErrorV("Unkown function referenced");

    //If argunment mismatch error
    if (CalleeF->arg_size() != Args.size())
        return LogErrorV("Incorrect # arguments passed");

    std::vector<Value *> ArgsV;
    for (unsigned i = 0, e=Args.size(); i != e ; ++i) {
        ArgsV.push_back(Args[i]->codegen());
        if(!ArgsV.back())
            return nullptr;
    }

    return Builder-> CreateCall(CalleeF, ArgsV, "calltmp");
}

Function *PrototypeAST::codegen() {
    std::vector<Type*> Doubles(Args.size(), Type::getDoubleTy(*TheContext));
    FunctionType *FT = 
        FunctionType::get(Type::getDoubleTy(*TheContext), Doubles, false);
    Function *F = 
        Function::Create(FT,Function::ExternalLinkage, Name, TheModule.get());

    //Set names for All Arguments
    unsigned Idx = 0;
    for (auto &Arg : F->args())
        Arg.setName(Args[Idx++]);
    return F;
}

//fix bug where extern defintation takes precedance and tha variable name  need to statu the same
Function *FunctionAST::codegen(){
    //First, check for an existing function from a previous extern declaration. 
    auto &P = *Proto;
    FunctionProtos[Proto->getName()] = std::move(Proto);
    Function *TheFunction = getFunction(P.getName()); 

    if (!TheFunction)
        return nullptr;

        //Create a new basic block to start insertion into 
    BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", TheFunction);
    Builder-> SetInsertPoint(BB);

    //Record the function arguments in the Namedvalues map.
    NamedValues.clear();

    for (auto &Arg : TheFunction->args())
        NamedValues[std::string(Arg.getName())] = &Arg;
    if(Value *RetVal = Body->codegen()) {
        //Finish off the function.
        Builder->CreateRet(RetVal);

        //Validate the generated code, checking for consistency
        verifyFunction(*TheFunction);

        //Optimize the function
        TheFPM->run(*TheFunction, *TheFAM);
        
        return TheFunction;
    }
   
    //Error Reading body, remove function
    TheFunction ->eraseFromParent();
    return nullptr;
}

