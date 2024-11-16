#ifndef AST_H
#define AST_H

#include <string>
#include <vector>

#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>

using namespace std;

/*
----PURPOSE:
    Declare all AST Expression node classes.
    There is 1 class for each node type (expressions, function prototypes, boolean expressions, etc.)
*/

// ----------------------------------------------------------------------------------------------
// ABSTRACT SYNTAX TREE =========================================================================
// ----------------------------------------------------------------------------------------------

// base class for all expression nodes

// each AST class has its own code generation (codegen()) method
// codegen() emits IR for last AST node and all the things it depends on.
// returns LLVM Value object -> represents Static Single Assignment (SSA)
class ExprAST
{
public:
    // virtual: lets this destructor be overridden in derived classes
    virtual ~ExprAST() = default;
    virtual llvm::Value *codegen() = 0;
};

// expression class for numeric literals ie. 1.0
class NumberExprAST : public ExprAST
{
    double Val;

public:
    NumberExprAST(double Val) : Val(Val) {} // constructor that sets value of Val to parameter Val
    llvm::Value *codegen() override;
};

// expression class for referencing a variable
class VariableExprAST : public ExprAST
{
    string Name;

public:
    VariableExprAST(const string &Name) : Name(Name) {}
    llvm::Value *codegen() override;
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

    llvm::Value *codegen() override;
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

    llvm::Value *codegen() override;
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

    llvm::Function *codegen();
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

    llvm::Function *codegen();
};

// expression class AST node for if/then/else -> pointers to subexpressions
class IfExprAST : public ExprAST
{
    unique_ptr<ExprAST> Cond, Then, Else;

public:
    IfExprAST(unique_ptr<ExprAST> Cond, unique_ptr<ExprAST> Then,
              unique_ptr<ExprAST> Else)
        : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}

    llvm::Value *codegen() override;
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

    llvm::Value *codegen() override;
};

#endif