import json
import time
from grok_lexer import Lexer 
from grok_compiler import Compiler
from grok_parser import Parser 
from grok_ast import Program 

from llvmlite import ir 
import llvmlite.binding as llvm  
from ctypes import CFUNCTYPE, c_int, c_float

LEXER_DEBUG: bool = True
PARSER_DEBUG: bool = True
COMPILER_DEBUG: bool = True
RUN_CODE: bool = True

if __name__ == "__main__":
    with open("tests/test.grk", "r") as f:
        code: str = f.read()

    if LEXER_DEBUG: 
        print("===== LEXER DEBUG =====")

        debug_lex: Lexer = Lexer(source=code)
        while debug_lex.current_char is not None: 
            print(debug_lex.next_token())

    l: Lexer = Lexer(source = code)
    p: Parser = Parser(lexer = l)
    program: Program = p.parse_program()

    if PARSER_DEBUG: 
        print("===== PARSER DEBUG =====")
        print(f'Program Statements: {program.stmts()}')
        
        with open("debug/ast.json", "w") as f: 
            json.dump(program.json(), f, indent=4)

        print("Wrote AST to debug/ast.json successfully")

    if len(p.errors) > 0: 
        for err in p.errors: 
            print(err)
        exit(1)

    c: Compiler = Compiler()
    c.compile(node=program)
    
    # Output Steps 
    module: ir.Module = c.module
    module.triple = llvm.get_default_triple()

    if COMPILER_DEBUG: 
        print("===== COMPILER DEBUG =====")
        with open("debug/ir.ll", "w") as f: 
            f.write(str(module))

    if RUN_CODE: 
        print("===== RUN CODE =====")
        llvm.initialize()
        llvm.initialize_native_target()
        llvm.initialize_native_asmprinter()

        try: 
            llvm_ir_parsed = llvm.parse_assembly(str(module)) # str(module) is the IR as a string
            llvm_ir_parsed.verify()
        except Exception as e:
            print(e)
            raise

        target_machine = llvm.Target.from_default_triple().create_target_machine()

        engine = llvm.create_mcjit_compiler(llvm_ir_parsed, target_machine)
        engine.finalize_object()

        entry = engine.get_function_address('main')
        cfunc = CFUNCTYPE(c_int)(entry) # create c function

        st = time.time()

        result = cfunc() # get whatever c function returns

        et = time.time()

        print(f'\n\nProgram returned: {result}\n=== Executed in {round((et - st) * 1000, 6)} ms. ===')
