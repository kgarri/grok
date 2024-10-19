#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <utility> 
#include <vector>

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
};

//NumberExprAST - Expression class for numeric literals like "1.0". 
class NumberExprAST : public ExprAST {
    double Val;
public: 
    NumberExprAST(double Val) : Val(Val) {}
};

//VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST {
    std::string Name;
public: 
    VariableExprAST(const std::string &Name) : Name(Name){}
};

// BinaryExprAST - Expression class for a binary operator
class BinaryExprAST : public ExprAST {
    char Op;
    std::unique_ptr<ExprAST> LHS, RHS;

public: 
    BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                  std::unique_ptr<ExprAST> RHS)
    : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
};

//CallExprAS - Expression class for function calls . 
class CallExprAST : public ExprAST {
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;
public: 
    CallExprAST(const std::string &Callee, 
                std::vector<std::unique_ptr<ExprAST>> Args)
    :Callee(Callee), Args(std::move(Args)) {}
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
};

//FunctionAST - This class represents a function definition itself.
class FunctionAST { 
    std::unique_ptr<PrototypeAST> Proto;
    std::unique_ptr<ExprAST> Body;
public:
    FunctionAST(std::unique_ptr<PrototypeAST> Proto,
                std::unique_ptr<ExprAST> Body)
    :Proto(std::move(Proto)), Body(std::move(Body)) {}
};

//-----------------------------------
//End of Abstract Syntax Tree
//-----------------------------------

//BinopPrecendence- This holds the precedence for each binary operator that is 
//defined
static std::map<char,int> BinopPrecedence;
static int CurTok;
std::unique_ptr<ExprAST> LogError(const char *Str);
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str);
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
static void HandleDefinition();
static void HandleExtern();
static void HandleTopLevelExpression();
static void MainLoop();

int main() {
    BinopPrecedence['<'] = 10;
    BinopPrecedence['+'] = 20;
    BinopPrecedence['-'] = 20;
    BinopPrecedence['*'] = 40;
    BinopPrecedence['/'] = 40;

    fprintf(stderr, "ready> ");
    getNextToken();

    MainLoop();
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



static void HandleDefinition() {
  if (ParseDefinition()) {
    fprintf(stderr, "Parsed a function definition.\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleExtern() {
  if (ParseExtern()) {
    fprintf(stderr, "Parsed an extern\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (ParseTopLevelExpr()) {
    fprintf(stderr, "Parsed a top-level expr\n");
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


