#pragma once

#include "grammar.h"
#include "string"
#include "vector"

struct token {
    enum terminal type;
    std::string text;
};

struct token_spec {
    enum terminal type;
    const char *regex;
};

inline constexpr token_spec token_specs[] = {
    {NUMBER,  "(0|1|2|3|4|5|6|7|8|9)(0|1|2|3|4|5|6|7|8|9)*"},
    {EQ,      "=="},
    {NEQ,     "!="},
    {LEQ,     "<="},
    {GEQ,     ">="},
    {LE,      "<"},
    {GE,      ">"},
    {PLUS,    "+"},
    {MINUS,   "-"},
    {MUL,     "\\*"},
    {DIV,     "/"},
    {LBRACE,  "\\("},
    {RBRACE,  "\\)"},
};

/* Lex a source string into a token list. */
std::vector<token> lex(const std::string &src);
