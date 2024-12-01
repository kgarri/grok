from enum import Enum 
from typing import Any

class TokenType(Enum):
    #Special 
    EOF = "EOF"
    ILLEGAL = "ILLEGAL"

    #Data Types 
    INT = "INT"
    FLOAT = "FLOAT"
    STRING = "STRING"

    # Arithematic Symbols 
    PLUS = "PLUS"
    MINUS = "MINUS"
    ASTERISK = "ASTRISK"
    SLASH = "SLASH"
    POW = "POW"
    MODULUS = "MODULUS"

    #Symbols 
    SEMICOLON = "SEMICOLON"
    LPAREN = "LPAREN"
    RPAREN = "RPAREN"

class Token: 
    def __init__(self, type: TokenType, literal: Any, line_no: int, position: int )-> None:
        self.type = type
        self.literal = literal 
        self.line_no = line_no
        self.position = position
    def __str__(self) -> str:
        return f"Token[{self.type} : {self.literal} : Line {self.line_no} : Position {self.position}]"
    def __repr__(self) -> str: 
        return str(self)

        

