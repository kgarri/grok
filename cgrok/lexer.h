#ifndef LEXER_H
#define LEXER_H

#include <string>

/*
----PURPOSE:
    1. Get next token
    2. Figure out what it is
    3. Figure out what to do with it
*/

extern std::string IdentifierStr; // used if tok_identifier
extern double NumVal;             // used if tok_number
extern std::string StrVal;        // used if tok_string

// returns [0-255] for unknown characters.
// returns the following for known things.
enum Token
{
    tok_eof = -1,

    // commands
    tok_def = -2,
    tok_extern = -3,

    // primary
    tok_identifier = -4,
    tok_number = -5,

    // control flow
    tok_if = -6,
    tok_then = -7,
    tok_else = -8,
    tok_for = -9,
    tok_in = -10,

    // other data types
    tok_string = -11
};

// gettok - Return next token from std. input
int gettok();

#endif
