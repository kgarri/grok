from llvmlite import ir 

from grok_ast import Node, NodeType, Program, Expression
from grok_ast import ExpressionStatement, LetStatement, BlockStatement, ReturnStatement, FunctionStatement, AssignStatement
from grok_ast import IntegerLiteral, FloatLiteral , IdentifierLiteral
from grok_ast import InfixExpression 
from grok_environment import Environment

class Compiler:
    def __init__(self) -> None: 
        self.type_map: dict[str, ir.Type] = {
            'int' : ir.IntType(32),
            'float' : ir.FloatType()
        }

        self.module: ir.Module = ir.Module('main')

        self.builder: ir.IRBuilder = ir.IRBuilder()

        self.env: Environment = Environment()

        # Temporary keeping track of errors 
        self.errors: list[str] = []


    def compile(self, node: Node) -> None: 
        if node == None: 
            return 
        match node.type(): 
            case NodeType.Program: 
                self.__visit_program(node)
            # Statements 
            case  NodeType.ExpressionStatement:
                self.__visit_expression_statement(node)
            case NodeType.LetStatement:
                self.__visit_let_statement(node)
            case NodeType.InfixExpression:
                self.__visit_infix_expression(node)
            case NodeType.FunctionStatement:
                self.__visit_function_statement(node)
            case NodeType.BlockStatement: 
                self.__visit_block_statement(node)
            case NodeType.ReturnStatement: 
                self.__visit_return_statement(node)
            case NodeType.AssignStatement: 
                self.__visit_assign_statement(node)
            

    # region Visit Methods 
    def __visit_program(self, node: Program) -> None: 
        for stmt in node.statements:
            self.compile(stmt)

    # region Statements
    def __visit_expression_statement(self, node: ExpressionStatement) -> None: 
        self.compile(node.expr)

    def __visit_let_statement(self, node: LetStatement) -> None:
        name: str = node.name.value
        value: Expression | None = node.value
        value_type: str = node.value_type
        value, Type  = self.__resolve_value(node=value)        


        if self.env.lookup(name) is None:
            #define and allocate
            ptr = self.builder.alloca(Type)

            # store ptr value
            self.builder.store(value, ptr)

            # add var to env
            self.env.define(name, ptr, Type)
        else: 
            ptr, _ = self.env.lookup(name)
            self.builder.store(value, ptr)

    def __visit_block_statement(self, node: BlockStatement) -> None: 
        for stmt in node.statements: 
            self.compile(stmt)

    def __visit_return_statement(self, node: ReturnStatement) -> None:
        value: Expression | None = node.return_value
        value, Type = self.__resolve_value(value)

        self.builder.ret(value)

    def __visit_function_statement(self, node: FunctionStatement) -> None:
        name: str = node.name.value
        body : BlockStatement | None = node.body
        params : list[IdentifierLiteral] = node.parameters

        # track param names
        param_names: list[str] = [p.value for p in params]

        # track types of params
        param_types: list[ir.Type] = []

        return_type: ir.Type = self.type_map[node.return_type]

        fnty: ir.FunctionType = ir.FunctionType(return_type, param_types)
        func: ir.Function = ir.Function(self.module, fnty, name=name)
    
        block: ir.Block = func.append_basic_block(f'{name}_entry')

        previous_builder = self.builder

        self.builder = ir.IRBuilder(block)

        previous_env = self.env

        self.env = Environment(parent=self.env)
        self.env.define(name, func, return_type)

        self.compile(body)

        self.env = previous_env
        self.env.define(name, func, return_type)

        self.builder = previous_builder
    def __visit_assign_statement(self, node: AssignStatement) -> None: 
        name: str | None = node.ident.value
        value: Expression = node.right_value

        value, Type = self.__resolve_value(value)

        if self.env.lookup(name) is None: 
            self.errors.append(f"COMPILE ERROR: Identifier: {name} has not been declared before it was re-assigned.")
        else: 
            ptr, _ = self.env.lookup(name)
            self.builder.store(value, ptr)


    #endregion 
    
    #region Expressions 
    def __visit_infix_expression(self, node: InfixExpression) -> tuple[ir.Value | None, ir.Type | None] :
        operator: str = node.operator
        left_value, left_type = self.__resolve_value(node.left_node)
        right_value, right_type = self.__resolve_value(node.right_node)
        
        value = None 
        Type = None 

        if isinstance(right_type, ir.IntType) and isinstance(left_type, ir.IntType):
            Type = self.type_map['int']
            match operator: 
                case '+':
                    value = self.builder.add(left_value, right_value)
                case'-':
                    value = self.builder.sub(left_value, right_value)
                case'*':
                    value = self.builder.mul(left_value, right_value)
                case'/': 
                    value = self.builder.sdiv(left_value, right_value)
                case '%':
                    # "srem" = "signed int remainder"
                    value = self.builder.srem(left_value, right_value)
                case '^':
                    # TODO: get exponents to work <3
                    pass

        elif isinstance(right_type, ir.FloatType) and isinstance(left_type, ir.FloatType):
            Type = self.type_map['float']
            match operator: 
                case '+':
                    value = self.builder.fadd(left_value, right_value)
                case'-':
                    value = self.builder.fsub(left_value, right_value)
                case'*':
                    value = self.builder.fmul(left_value, right_value)
                case'/': 
                    value = self.builder.fdiv(left_value, right_value)
                case '%':
                    # "srem" = "signed int remainder"
                    value = self.builder.srem(left_value, right_value)
                case '^':
                    # TODO: get exponents to work <3
                    pass

        return value, Type 
    #endregion

    #endregion

    #region Helper Methods
    def __resolve_value(self, node: Expression, value_type: str = None) -> tuple[ir.Value, ir.Type]:
        match node.type(): 
            case NodeType.IntegerLiteral:  
                value, Type = node.value, self.type_map["int" if value_type is None else value_type]
                return ir.Constant(Type , value), Type 
            case NodeType.FloatLiteral: 
                value, Type = node.value, self.type_map["float" if value_type is None else value_type]
                return ir.Constant(Type , value), Type 
            case NodeType.IdentifierLiteral: 
                ptr,Type = self.env.lookup(node.value)
                return self.builder.load(ptr), Type

            # Expression Values
            case NodeType.InfixExpression: 
                return self.__visit_infix_expression(node)
            

    #endregion
