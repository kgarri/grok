#include "lexer.h"

#include <string>
#include <vector>

#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>

using namespace std;

// ----------------------------------------------------------------------------------------------
// LEXER ========================================================================================
// ----------------------------------------------------------------------------------------------

std::string IdentifierStr; // used if tok_identifier
double NumVal;             // used if tok_number

// gettok - Return next token from std. input
int gettok()
{
    // TODO: enforce formatting rules here

    static int LastChar = ' ';

    // skip whitespace
    // reads characters one at a time from stdin
    // eats them as it reads them, stores last char red (but not processed) in LastChar
    while (isspace(LastChar))
        LastChar = getchar();

    // if LastChar is a letter, it's part of an identifier
    if (isalpha(LastChar)) // identifier: a-z, A-Z, 0-9
    {
        IdentifierStr = LastChar;

        // get the full identifier
        // isalnum() checks if a char is a decimal digit OR an upper/lowercase letter
        while (isalnum((LastChar = getchar())))
            IdentifierStr += LastChar;

        // if token is "def" or "extern," return those corresponding tokens
        // else return that it is an identifier
        if (IdentifierStr == "def")
            return tok_def;
        if (IdentifierStr == "extern")
            return tok_extern;
        if (IdentifierStr == "if")
            return tok_if;
        if (IdentifierStr == "then")
            return tok_then;
        if (IdentifierStr == "else")
            return tok_else;
        if (IdentifierStr == "for")
            return tok_for;
        if (IdentifierStr == "in")
            return tok_in;
        return tok_identifier;
    }

    // if char is any of the things that make up a double - a number or a decimal, it's a number
    if (isdigit(LastChar) || LastChar == '.')
    { // number: 0-9. +
        string NumStr;

        // TODO: limit to 1 decimal. truncate any that remain.
        do
        {
            NumStr += LastChar;
            LastChar = getchar();

        } while (isdigit(LastChar) || LastChar == '.');

        NumVal = strtod(NumStr.c_str(), nullptr); // convert from str to double, store in NumVal
        return tok_number;                        // return that it is in fact a number
    }

    // comments start with  '?'
    if (LastChar == '?')
    {
        // comment lasts until end of line
        do
        {
            LastChar = getchar();
        } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r'); // all possible EOFs, including the one defined by this language

        if (LastChar != EOF)
            return gettok();
    }

    // all other cases
    // check for end of file. don't eat EOF using getchar. that'd be bad...?
    if (LastChar == EOF)
        return tok_eof;

    // otherwise return char as its ascii value, we dk what else to do with it
    int ThisChar = LastChar;
    LastChar = getchar();
    return ThisChar;
}