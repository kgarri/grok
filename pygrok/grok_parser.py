from grok_lexer import Lexer
from grok_token import Token, TokenType 
from typing import Callable
from enum import Enum, auto

from grok_ast import Statement, Expression, Program
from grok_ast import ExpressionStatement, LetStatement, FunctionStatement, ReturnStatement, BlockStatement, AssignStatement, IfStatement
from grok_ast import InfixExpression, CallExpression
from grok_ast import IntegerLiteral, FloatLiteral, StringLiteral, IdentifierLiteral, BooleanLiteral

# precedence types 
class PrecedenceType(Enum):
    P_LOWEST = 0
    P_EQUALS = auto()
    P_LESSGREATER = auto() 
    P_SUM = auto()
    P_PRODUCT = auto()
    P_EXPONENT = auto()
    P_PREFIX = auto()
    P_CALL = auto()
    P_INDEX = auto()

# precedence mapping
PRECEDENCES: dict[TokenType, PrecedenceType] = {
    TokenType.PLUS: PrecedenceType.P_SUM,
    TokenType.MINUS: PrecedenceType.P_SUM, 
    TokenType.SLASH: PrecedenceType.P_PRODUCT, 
    TokenType.ASTERISK: PrecedenceType.P_PRODUCT, 
    TokenType.MODULUS: PrecedenceType.P_PRODUCT, 
    TokenType.POW: PrecedenceType.P_EXPONENT, 
    TokenType.EQ_EQ: PrecedenceType.P_EQUALS, 
    TokenType.NOT_EQ: PrecedenceType.P_EQUALS, 
    TokenType.LT: PrecedenceType.P_LESSGREATER, 
    TokenType.LT_EQ: PrecedenceType.P_LESSGREATER, 
    TokenType.GT: PrecedenceType.P_LESSGREATER, 
    TokenType.GT_EQ: PrecedenceType.P_LESSGREATER,
    TokenType.NOT: PrecedenceType.P_PREFIX
}

class Parser: 
    def __init__(self, lexer: Lexer) -> None:
        self.lexer: Lexer = lexer

        self.errors: list[str] = []

        self.current_token: Token = Token() # current tok
        self.peek_token: Token = Token() # next tok

        self.prefix_parse_fns: dict[TokenType, Callable] = {
            TokenType.IDENT: self.__parse_identifier,
            TokenType.INT: self.__parse_int_literal,
            TokenType.FLOAT: self.__parse_float_literal,
            TokenType.STRING: self.__parse_string_literal,
            TokenType.LPAREN: self.__parse_grouped_expression, 
            TokenType.IF: self.__parse_if_statement, 
            TokenType.TRUE: self.__parse_boolean,
            TokenType.FALSE: self.__parse_boolean,
        } # ie "-"
        self.infix_parse_fns: dict[TokenType, Callable] = {
            TokenType.PLUS: self.__parse_infix_expression,
            TokenType.MINUS: self.__parse_infix_expression,
            TokenType.SLASH: self.__parse_infix_expression,
            TokenType.ASTERISK: self.__parse_infix_expression,
            TokenType.POW: self.__parse_infix_expression,
            TokenType.MODULUS: self.__parse_infix_expression,
            TokenType.EQ_EQ: self.__parse_infix_expression, 
            TokenType.NOT_EQ: self.__parse_infix_expression, 
            TokenType.LT: self.__parse_infix_expression, 
            TokenType.LT_EQ: self.__parse_infix_expression, 
            TokenType.GT: self.__parse_infix_expression, 
            TokenType.GT_EQ: self.__parse_infix_expression
        } # ie. operators

        self.__next_token()
        self.__next_token()

    # region Parser Helpers
    # get next token 
    def __next_token(self) -> None: 
        self.current_token = self.peek_token 
        self.peek_token = self.lexer.next_token()
    def __current_token_is(self, tt: TokenType) -> bool: 
        return self.current_token.type == tt 

    # get bool for if peek_token's type is TokenType
    def __peek_token_is(self, tt: TokenType) -> bool: 
        return self.peek_token.type == tt
    
    # check that peek token is of the correct type, if not throw an error
    def __expect_peek(self, tt: TokenType) -> bool: 
        if self.__peek_token_is(tt):
            self.__next_token()
            return True
        else:
            self.__peek_error(tt)
            return False 

    # check precedence (current)
    def __current_precedence(self) -> PrecedenceType:
        if self.peek_token.type == None: 
            return PrecedenceType.P_LOWEST
        prec: PrecedenceType | None = PRECEDENCES.get(self.peek_token.type)
        if prec is None: 
            return PrecedenceType.P_LOWEST
        return prec 
    # check precedence (next)
    def __peek_precedence(self) -> PrecedenceType:
        if self.peek_token.type == None: 
            return PrecedenceType.P_LOWEST
        prec: PrecedenceType | None = PRECEDENCES.get(self.peek_token.type)
        if prec is None: 
            return PrecedenceType.P_LOWEST
        return prec 

    # append error to errors list 
    def __peek_error(self, tt: TokenType) -> None: 
        self.errors.append(f'Expected next token to be {tt}, got {self.peek_token.type} instead.')
    
    # used when checking prefix parse function - checks that token exists
    def __no_prefix_parse_fn_error(self, tt: TokenType) -> None: 
        self.errors.append(f'No Prefix Parse function for {tt} found.')
    
    # endregion 

    def parse_program(self) -> Program: 
        program: Program = Program()

        while self.current_token.type != TokenType.EOF:
            stmt: Statement | None = self.__parse_statement()
            if stmt is not None: 
                program.statements.append(stmt)

            self.__next_token()

        print("Parsed entire program.")
        return program 
    
    # region Statement Methods
    def __parse_statement(self) -> Statement | None:
        if self.current_token.type == TokenType.IDENT and self.__peek_token_is(TokenType.EQ): 
            return self.__parse_assignment_statement()
        match self.current_token.type:
            case TokenType.LET: 
                return self.__parse_let_statement()
            case TokenType.FN: 
                return self.__parse_function_statement()
            case TokenType.RETURN: 
                return self.__parse_return_statement()
            case _:
                return self.__parse_expression_statement()
    
    def __parse_expression_statement(self) -> ExpressionStatement:
        expr = self.__parse_expression(PrecedenceType.P_LOWEST)

        if self.__peek_token_is(TokenType.SEMICOLON):
            self.__next_token()

        print(f'self: {self} expr: {expr}')
        stmt: ExpressionStatement = ExpressionStatement(expr=expr)

        return stmt
    
    def __parse_let_statement(self) -> LetStatement | None: 
        #let a: int = 10; 
        stmt: LetStatement = LetStatement()

        if not self.__expect_peek(TokenType.IDENT):
            return None

        stmt.name = IdentifierLiteral(value = self.current_token.literal)

        if not self.__expect_peek(TokenType.COLON):
            return None
        
        if not self.__expect_peek(TokenType.TYPE):
            return None 
        
        stmt.value_type = self.current_token.literal 

        if not self.__expect_peek(TokenType.EQ):
            return None

        self.__next_token()

        stmt.value = self.__parse_expression(PrecedenceType.P_LOWEST)

        while not self.__current_token_is(TokenType.SEMICOLON) and not self.__current_token_is(TokenType.EOF): 
            self.__next_token()

        return stmt

    def __parse_function_statement(self) -> FunctionStatement | None:
        stmt: FunctionStatement = FunctionStatement()

        # fn name() 8> int { return 10; }

        if not self.__expect_peek(TokenType.IDENT):
            print("ERROR: NO IDENTIFIER")
            return None
        
        stmt.name = IdentifierLiteral(value=self.current_token.literal)

        if not self.__expect_peek(TokenType.LPAREN):
            print("ERROR: NO LPAREN")
            return None
        
        stmt.parameters = []
        if not self.__expect_peek(TokenType.RPAREN):
            print("ERROR: NO RPAREN")
            return None
        
        if not self.__expect_peek(TokenType.ARROW):
            print("ERROR: NO ARROW")
            return None
        
        if not self.__expect_peek(TokenType.TYPE):
            print("ERROR: NO TYPE")
            return None
        
        stmt.return_type = self.current_token.literal

        if not self.__expect_peek(TokenType.LBRACE):
            print('ERROR: FOUND NO LBRACE.')
            return None
        
        stmt.body = self.__parse_block_statement()

        return stmt 

    def __parse_return_statement(self) -> ReturnStatement | None:
        stmt: ReturnStatement = ReturnStatement()

        self.__next_token()

        stmt.return_value = self.__parse_expression(PrecedenceType.P_LOWEST)

        if not self.__expect_peek(TokenType.SEMICOLON):
            return None
        
        return stmt

    def __parse_block_statement(self) -> BlockStatement:
        block_stmt: BlockStatement = BlockStatement()

        self.__next_token()

        while not self.__current_token_is(TokenType.RBRACE) and not self.__current_token_is(TokenType.EOF):
            stmt: Statement | None  = self.__parse_statement()
            if stmt is not None: 
                block_stmt.statements.append(stmt)

            self.__next_token()

        return block_stmt
    def __parse_assignment_statement(self) -> AssignStatement | None: 
        stmt: AssignStatement = AssignStatement()

        stmt.ident = IdentifierLiteral(value = self.current_token.literal)

        self.__next_token()
        self.__next_token()

        stmt.right_value = self.__parse_expression(PrecedenceType.P_LOWEST)

        self.__next_token()

        return stmt
    def __parse_if_statement(self) -> IfStatement | None: 
        stmt: IfStatement = IfStatement ()
        self.__next_token()

        stmt.condition = self.__parse_expression(PrecedenceType.P_LOWEST)

        if not self.__expect_peek(TokenType.LBRACE): 
            return None 
        stmt.consequence = self.__parse_block_statement()

        if  self.__peek_token_is(TokenType.ELIF):
            self.__next_token()
            stmt.alternative = self.__parse_if_statement()
        elif self.__peek_token_is(TokenType.ELSE): 
            self.__next_token()
            if not self.__expect_peek(TokenType.LBRACE): 
                return None 
            stmt.alternative = self.__parse_block_statement()
        
        return stmt
    # endregion

    # region Expression Methods
    def __parse_expression(self, precedence: PrecedenceType) -> Expression | None:
        if self.current_token.type == None:
            return None
        prefix_fn: Callable | None = self.prefix_parse_fns.get(self.current_token.type)
        if prefix_fn is None: 
            self.__no_prefix_parse_fn_error(self.current_token.type)
            return None
        
        left_expr: Expression = prefix_fn()
        while not self.__peek_token_is(TokenType.SEMICOLON) and precedence.value < self.__peek_precedence().value:
            if self.peek_token.type == None:
                return left_expr
            infix_fn: Callable | None = self.infix_parse_fns.get(self.peek_token.type)
            if infix_fn is None:
                return left_expr
            
            self.__next_token()

            left_expr = infix_fn(left_expr)

        return left_expr

    def __parse_infix_expression(self, left_node: Expression) -> Expression:
        infix_expr: InfixExpression = InfixExpression(left_node=left_node, operator=self.current_token.literal)

        precedence = self.__current_precedence()

        self.__next_token()

        infix_expr.right_node = self.__parse_expression(precedence)
        return infix_expr
    
    def __parse_grouped_expression(self) -> Expression | None:
        self.__next_token()

        expr: Expression | None = self.__parse_expression(PrecedenceType.P_LOWEST)

        if not self.__expect_peek(TokenType.RPAREN):
            return None
        
        return expr

    def __parse_call_expression(self, function: Expression) -> CallExpression:
        expr: CallExpression = CallExpression(function=function)
        expr.arguments = [] # todo function args
        
        if not self.__expect_peek(TokenType.RPAREN):
            return None
        
        return expr

    # endregion

    # region Prefix Methods
    def __parse_identifier(self) -> IdentifierLiteral | None: 
        return IdentifierLiteral(value=self.current_token.literal)

    def __parse_string_literal(self) -> Expression | None:
        """Parses StringLiteral from current token"""
        str_lit: StringLiteral = StringLiteral()

        try: 
            str_lit.value = self.current_token.literal
        except:
            self.errors.append("Could not parse `(self.current_token.literal)` as a string.")
            return None
        
        return str_lit

    def __parse_int_literal(self) -> Expression | None:
        """Parses IntegerLiteral from current token"""
        int_lit: IntegerLiteral = IntegerLiteral()

        try: 
            int_lit.value = int(self.current_token.literal)
        except:
            self.errors.append("Could not parse `(self.current_token.literal)` as an integer.")
            return None
        
        return int_lit
    
    def __parse_float_literal(self) -> Expression | None:
        """Parses FloatLiteral from current token"""
        float_lit: FloatLiteral = FloatLiteral()

        try: 
            float_lit.value = int(self.current_token.literal)
        except:
            self.errors.append("Could not parse `(self.current_token.literal)` as a float.")
            return None
        
        return float_lit

    def __parse_boolean(self) -> BooleanLiteral: 
        return BooleanLiteral(value = self.__current_token_is(TokenType.TRUE))
    
    # endregion
