#include "parser.h"
#include "ast.h"   // contains ExprAST and other classes
#include "lexer.h" // contains gettok()
#include "codegen.h"

#include <memory> // used for unique_ptr
#include <map>

using namespace std; // used for unique_ptr

// ----------------------------------------------------------------------------------------------
// PARSER ========================================================================================
// ----------------------------------------------------------------------------------------------

// CurTok/getNextToken - provide token buffer around lexer
int CurTok;
int getNextToken()
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

// called when token is tok_string
// takes current value, makes StringExprAST node, advances, returns
static unique_ptr<ExprAST> ParseStrExpr()
{
    auto Result = make_unique<StringExprAST>(StrVal); // make a string with the value
    getNextToken();
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
    case tok_string:
        return ParseStrExpr();
    }
}

// operator precendence parsing
// holds precedence for every binary operator defined
map<char, int> BinopPrecedence;

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
unique_ptr<FunctionAST> ParseDefinition()
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
unique_ptr<PrototypeAST> ParseExtern()
{
    getNextToken(); // eat 'extern'
    return ParsePrototype();
}

// ::= expression
unique_ptr<FunctionAST> ParseTopLevelExpr()
{
    if (auto E = ParseExpression())
    {
        // anonymous proto
        auto Proto = make_unique<PrototypeAST>("__anon_expr", vector<string>());
        return make_unique<FunctionAST>(std::move(Proto), std::move(E));
    }
    return nullptr;
}
