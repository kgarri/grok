#ifndef TOPLEVEL_H
#define TOPLEVEL_H

#include "codegen.h"
#include "parser.h"
#include "lexer.h"

using namespace llvm;
using namespace llvm::orc;

void InitializeModuleAndManagers();
void HandleDefinition();
void HandleExtern();
void HandleTopLevelExpression();

// definition | external | expression | ';'
void MainLoop();

#endif