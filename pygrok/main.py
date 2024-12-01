import json
from grok_lexer import Lexer 
from grok_compiler import Compiler
from grok_parser import Parser 
from grok_ast import Program 
import json

from llvmlite import ir 
import llvmlite.binding as llvm  
from ctypes import CFUNCTYPE, c_int, c_float 

LEXER_DEBUG: bool = False
PARSER_DEBUG: bool = True
COMPILER_DEBUG: bool = True

if __name__ == "__main__":
    with open("tests/parser.grk", "r") as f:
        code: str = f.read()

    if LEXER_DEBUG: 
        print("===== LEXER  DEBUG =====")
        
LEXER_DEBUG: bool = True
PARSER_DEBUG: bool = True

if __name__ == "__main__":
    with open("tests/parser.gr", "r") as f:
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
        with open("debug/ir.ll", "w") as f: 
            f.write(str(module))
