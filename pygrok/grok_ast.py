from abc import ABC, abstractmethod
from enum import Enum

class NodeType(Enum):
    Program = "Program"

    # Statements
    ExpressionStatement = "ExpressionStatement"
    LetStatement = "LetStatement"
    FunctionStatement = "FunctionStatement"
    BlockStatement = "BlockStatement"
    ReturnStatement = "ReturnStatement"

    # Expressions
    InfixExpression = "InfixExpression"

    # Literals
    IntegerLiteral = "IntegerLiteral"
    FloatLiteral = "FloatLiteral"
    StringLiteral = "StringLiteral"
    IdentifierLiteral = "IdentifierLiteral"

class Node(ABC):
    @abstractmethod
    def type(self) -> NodeType: 
        pass

    @abstractmethod
    # debugging purposes only
    def json(self) -> dict: 
        pass

class Statement(Node):
    pass

class Expression(Node):
    pass

class Program(Node):
    def __init__(self):
        self.statements: list[Statement] = []

    def stmts(self) -> list[Statement]:
        result = []
        for stmt in self.statements:
            result.append(str(stmt))
        return result

    def type(self) -> NodeType:
        return NodeType.Program
    
    def json(self) -> dict:
        return{
            "type": self.type().value,
            "statements": [{stmt.type().value: stmt.json()} for stmt in self.statements]
        }
    
# region Statements
class ExpressionStatement(Statement):
    def __init__(self, expr: Expression | None  = None) -> None:
        self.expr: Expression | None  = expr

    def __str__(self):
        return f'ExpressionStatement({self.expr})'

    def type(self) -> NodeType: 
        return NodeType.ExpressionStatement
    
    def json(self) -> dict:
        return {
            "type": self.type().value, 
            "expr": self.expr.json() if self.expr != None else "None"
        }
    
class LetStatement(Statement): 
    def __init__(self, name: Expression | None = None, value: Expression | None = None, value_type: str | None = None) -> None: 
        self.name = name 
        self.value = value 
        self.value_type = value_type
    
    def __str__(self):
        return f'LetStatement(name={self.name}, value={self.value},value type={self.value_type})'
    
    def type(self) -> NodeType: 
        return NodeType.LetStatement

    def json(self) -> dict: 
        return {
            "type": self.type().value, 
            "name": self.name.json() if self.name != None else "None", 
            "value": self.value.json() if self.value != None else "None", 
            "value_type": self.value_type
        }

class BlockStatement(Statement):
    def __init__(self, statements: list[Statement] | None= None) -> None:
        self.statements = statements if statements is not None else []
    
    def __str__(self):
        return f'BlockStatement({self.statements})'

    def type(self) -> NodeType:
        return NodeType.BlockStatement
    
    def json(self) -> dict: 
        return {
            "type": self.type().value, 
            "statements": [stmt.json() for stmt in self.statements]
        }
    
class ReturnStatement(Statement):
    def __init__(self, return_value: Expression | None = None) -> None:
        self.return_value = return_value

    def __str__(self):
        return f'ReturnStatement({str(self.return_value)})'
    
    def type(self) -> NodeType:
        return NodeType.ReturnStatement
    
    def json(self) -> dict:
        return {
            "type": self.type().value,
            "return_value": self.return_value.json() if self.return_value != None else "None"
        }
    
class FunctionStatement(Statement):
    def __init__(self, parameters: list = [], body: BlockStatement | None = None, name = None, return_type: str | None = None) -> None:
        self.parameters = parameters
        self.body = body
        self.name = name
        self.return_type = return_type

    def __str__(self):
        return f'FunctionStatement(name={self.name}, parameters={self.parameters}, body={self.body}, return type={self.return_type})'

    def type(self) -> NodeType:
        return NodeType.FunctionStatement
    
    def json(self) -> dict:
        return {
            "type": self.type().value,
            "name": self.name.json() if self.name != None else "None",
            "return_type": self.return_type, 
            "parameters": [p.json() for p in self.parameters],
            "body": self.body.json() if self.body != None else "None"
       }
# endregion

# region Expressions
class InfixExpression(Expression):
    def __init__(self, left_node: Expression, operator: str, right_node: Expression | None = None) -> None:
        self.left_node: Expression = left_node
        self.operator: str = operator
        self.right_node: Expression | None = right_node

    def type(self) -> NodeType:
        return NodeType.InfixExpression
    
    def json(self) -> dict:
        return {
            "type": self.type().value,
            "left_node": self.left_node.json(),
            "operator": self.operator, 
            "right_node": self.right_node.json() if self.right_node != None else "None"
        }
# endregion

# region Literals
class IntegerLiteral(Expression):
    def __init__(self, value: int | None = None) -> None:
        self.value: int | None = value

    def __str__(self):
        return f'IntegerLiteral({self.value})'
    
    def type(self) -> NodeType: 
        return NodeType.IntegerLiteral
    
    def json(self) -> dict: 
        return {
            "type": self.type().value,
            "value": self.value
        }

class FloatLiteral(Expression):
    def __init__(self, value: float | None = None) -> None:
        self.value: float | None = value

    def __str__(self):
        return f'FloatLiteral({self.value})'
   
    def type(self) -> NodeType: 
        return NodeType.FloatLiteral
    
    def json(self) -> dict: 
        return {
            "type": self.type().value,
            "value": self.value
        }
    
class StringLiteral(Expression):
    def __init__(self, value: str | None = None) -> None:
        self.value: str | None = value
 
    def __str__(self):
        return f'StringLiteral({self.value})'
   
    def type(self) -> NodeType: 
        return NodeType.StringLiteral
    
    def json(self) -> dict: 
        return {
            "type": self.type().value,
            "value": self.value
        }
class IdentifierLiteral(Expression):
    def __init__(self, value: str | None = None) -> None:
        self.value: str | None = value
    def __str__(self):
        return f'IdentifierLiteral({self.value})'

    def type(self) -> NodeType: 
        return NodeType.IdentifierLiteral
    
    def json(self) -> dict: 
        return {
            "type": self.type().value,
            "value": self.value
        }
# endregion
