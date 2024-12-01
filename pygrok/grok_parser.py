from grok_lexer import Lexer
from grok_token import Token, TokenType 
from typing import Callable
from enum import Enum, auto

from grok_ast import Statement, Expression, Program
from grok_ast import ExpressionStatement, LetStatement, FunctionStatement, ReturnStatement, BlockStatement
from grok_ast import InfixExpression
from grok_ast import IntegerLiteral, FloatLiteral, StringLiteral, IdentifierLiteral

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
    TokenType.POW: PrecedenceType.P_EXPONENT
}

class Parser: 
    def __init__(self, lexer: Lexer) -> None:
        self.lexer: Lexer = lexer

        self.errors: list[str] = []

        self.current_token: Token = None # current tok
        self.peek_token: Token = None # next tok

        self.prefix_parse_fns: dict[TokenType, Callable] = {
            TokenType.IDENT: self.__parse_identifier,
            TokenType.INT: self.__parse_int_literal,
            TokenType.FLOAT: self.__parse_float_literal,
            TokenType.STRING: self.__parse_string_literal,
            TokenType.LPAREN: self.__parse_grouped_expression
        } # ie "-"
        self.infix_parse_fns: dict[TokenType, Callable] = {
            TokenType.PLUS: self.__parse_infix_expression,
            TokenType.MINUS: self.__parse_infix_expression,
            TokenType.SLASH: self.__parse_infix_expression,
            TokenType.ASTERISK: self.__parse_infix_expression,
            TokenType.POW: self.__parse_infix_expression,
            TokenType.MODULUS: self.__parse_infix_expression
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
            return False; 

    # check precedence (current)
    def __current_precedence(self) -> PrecedenceType:
        prec: int | None = PRECEDENCES.get(self.peek_token.type)
        if prec is None: 
            return PrecedenceType.P_LOWEST
        return 
    
    # check precedence (next)
    def __peek_precedence(self) -> PrecedenceType:
        prec: int | None = PRECEDENCES.get(self.peek_token.type)
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

    def parse_program(self) -> None: 
        program: Program = Program()

        while self.current_token.type != TokenType.EOF:
            stmt: Statement = self.__parse_statement()
            if stmt is not None: 
                program.statements.append(stmt)

            self.__next_token()

        return program 
    
    # region Statement Methods
    def __parse_statement(self) -> Statement:
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

        stmt: ExpressionStatement = ExpressionStatement(expr=expr)

        return stmt
    
    def __parse_let_statement(self) -> LetStatement: 
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

    def __parse_function_statement(self) -> FunctionStatement:
        stmt: FunctionStatement = FunctionStatement()

        # fn name() 8> int { return 10; }

        if not self.__expect_peek(TokenType.IDENT):
            return None
        
        stmt.name = IdentifierLiteral(value=self.current_token.literal)

        if not self.__expect_peek(TokenType.LPAREN):
            return None
        
        stmt.parameters = []
        if not self.__expect_peek(TokenType.RPAREN):
            return None
        
        if not self.__expect_peek(TokenType.ARROW):
            return None
        
        if not self.__expect_peek(TokenType.TYPE):
            return None
        
        stmt.return_type = self.current_token.literal

        if not self.__expect_peek(TokenType.LBRACE):
            return None
        
        stmt.body = self.__parse_block_statement()

        return stmt 

    def __parse_return_statement(self) -> ReturnStatement:
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
            stmt: Statement = self.__parse_statement()
            if stmt is not None: 
                block_stmt.statements.append(stmt)

            self.__next_token()

        return block_stmt
    # endregion

    # region Expression Methods
    def __parse_expression(self, precedence: PrecedenceType) -> Expression:
        
        prefix_fn: Callable | None = self.prefix_parse_fns.get(self.current_token.type)
        if prefix_fn is None: 
            self.__no_prefix_parse_fn_error(self.current_token.type)
            return None
        
        left_expr: Expression = prefix_fn()
        while not self.__peek_token_is(TokenType.SEMICOLON) and precedence.value < self.__peek_precedence().value:
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
    
    def __parse_grouped_expression(self) -> Expression:
        self.__next_token()

        expr: Expression = self.__parse_expression(PrecedenceType.P_LOWEST)

        if not self.__expect_peek(TokenType.RPAREN):
            return None
        
        return expr

    # endregion

    # region Prefix Methods
    def __parse_identifier(self) -> IdentifierLiteral: 
        return IdentifierLiteral(value=self.current_token.literal)

    def __parse_string_literal(self) -> Expression:
        """Parses StringLiteral from current token"""
        str_lit: StringLiteral = StringLiteral()

        try: 
            str_lit.value = int(self.current_token.literal)
        except:
            self.errors.append("Could not parse `(self.current_token.literal)` as a string.")
            return None
        
        return str_lit

    def __parse_int_literal(self) -> Expression:
        """Parses IntegerLiteral from current token"""
        int_lit: IntegerLiteral = IntegerLiteral()

        try: 
            int_lit.value = int(self.current_token.literal)
        except:
            self.errors.append("Could not parse `(self.current_token.literal)` as an integer.")
            return None
        
        return int_lit
    
    def __parse_float_literal(self) -> Expression:
        """Parses FloatLiteral from current token"""
        float_lit: FloatLiteral = FloatLiteral()

        try: 
            float_lit.value = int(self.current_token.literal)
        except:
            self.errors.append("Could not parse `(self.current_token.literal)` as a float.")
            return None
        
        return float_lit
    
    # endregion