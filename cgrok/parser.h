#ifndef PARSER_H
#define PARSER_H

#include "ast.h"

#include <memory> // used for unique_ptr
#include <map>

using namespace std; // used for unique_ptr

// operator precendence parsing
// holds precedence for every binary operator defined
extern map<char, int> BinopPrecedence;

static unique_ptr<ExprAST> ParseExpression();
static unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, unique_ptr<ExprAST> LHS);

// CurTok/getNextToken - provide token buffer around lexer
extern int CurTok;
int getNextToken();

// error handling helper functions
unique_ptr<ExprAST> LogError(const char *Str);
unique_ptr<PrototypeAST> LogErrorP(const char *Str);

// numberexpr ::= number
// called when token is tok_number
// takes current value, makes a NumberExprAST node, advances, returns
static unique_ptr<ExprAST> ParseNumberExpr();
static unique_ptr<ExprAST> ParseStrExpr();

static unique_ptr<ExprAST> ParseParenExpr(); // parenexpr ::= '(' expression ')'

// ::= identifier
// ::= identifier '(' expression* ')'
// handles variable references and function calls
// called if current token is a tok_identifier token, recursion and error handling
static unique_ptr<ExprAST> ParseIdentifierExpr();

static unique_ptr<ExprAST> ParseIfExpr();
static unique_ptr<ExprAST> ParseForExpr();

// primary
//  ::= identifierexpr
//  ::= numberexpr
//  ::= parenexpr
// look at an expression that can be any of the 3 above (primary expressions) and decide which one it is
static unique_ptr<ExprAST> ParsePrimary();

static int GetTokPrecendence(); // get precedence of preceding binary op token

static unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, unique_ptr<ExprAST> LHS); // ::= ('+' primary)*

static std::unique_ptr<ExprAST> ParseExpression();

// prototype
//  ::= id '(' id* ')'
static unique_ptr<PrototypeAST> ParsePrototype();

// definition ::= 'def' prototype expression
// function def = prototype wwith expression to implement the body
unique_ptr<FunctionAST> ParseDefinition();

// external ::= 'extern' prototype
// prototype with no body
unique_ptr<PrototypeAST> ParseExtern();

// ::= expression
unique_ptr<FunctionAST> ParseTopLevelExpr();

#endif
