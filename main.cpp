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

#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "ast.h"
#include "toplevel.h"

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

// printstr - printf that takes string and prints with newline, returns 0
extern "C" DLLEXPORT char printstr(char S[])
{
    fprintf(stderr, "%s\n", S);
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
    BinopPrecedence['>'] = 10;
    BinopPrecedence['+'] = 20;
    BinopPrecedence['-'] = 20;
    BinopPrecedence['%'] = 40;
    BinopPrecedence['/'] = 40;
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
