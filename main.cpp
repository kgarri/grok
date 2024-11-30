#include "KaleidoscopeJIT.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Support/TargetSelect.h"
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <map>
#include <memory>

#include "parser.h"
#include "codegen.h"
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
#if defined(_MSC_VER)
    #define EXPORT __declspec(dllexport)
    #define IMPORT __declspec(dllimport)
#elif defined(__GNUC__)
    #define EXPORT __attribute__((visibility("default")))
    #define IMPORT
#else
    #define EXPORT
    #define IMPORT
#endif



// PRODUCE CONSOLE OUTPUT USING C ----

// putchard - putchar takes a double ASCII value and returns 0
// writes a single char from ASCII to stdout 

extern "C" EXPORT double putchard(double X)
{
    fputc((char)X, stderr);
    return 0;
}


// printd - printf that takes a double and prints with new line, returns 0
extern "C" EXPORT double printd(double X)
{
    fprintf(stderr, "%f\n", X);
    return 0;
}

// printstr - printf that takes string and prints with newline, returns 0
extern "C" EXPORT char printstr(char S[])
{
    fprintf(stderr, "%s\n", S);
    return 0;
}

extern "C" EXPORT char* concatstr(char S1[], char S2[])
{
     int lengthOfStr1 = strlen(S1);
     int lengthOfStr2 = strlen(S2);
     char *result = (char*)malloc(lengthOfStr1 + lengthOfStr2);
     int i= 0;
     printf("%s\n", S1);
     while (S1[i] != '\0') {
        if (result[i] == '\0') {
            result[i] = S1[i];
        }
        i++;
    }
     strcat(result, S2);
     return result;
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
