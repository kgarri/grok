#include "codegen.h"
#include "parser.h"
#include "lexer.h"

using namespace llvm;
using namespace llvm::orc;

// ----------------------------------------------------------------------------------------------
// TOP LEVEL PARSING + JIT Driver ==============================================================
// ----------------------------------------------------------------------------------------------

void InitializeModuleAndManagers()
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

void HandleDefinition()
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

void HandleExtern()
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
void HandleTopLevelExpression()
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
            if (double (*FP)() = ExprSymbol.getAddress().toPtr<double (*)()>())
                fprintf(stderr, "Evaluated to %f\n", FP());
            if (string (*FP)() = ExprSymbol.getAddress().toPtr<string (*)()>())
                FP();

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
void MainLoop()
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
