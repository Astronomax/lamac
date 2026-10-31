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
    {VAR,         "var"},
    {READ,        "read"},
    {WRITE,       "write"},
    {SKIP,        "skip"},
    {TRUE,        "true"},
    {FALSE,       "false"},
    {LIDENT,      "(a|b|c|d|e|f|g|h|i|j|k|l|m|n|o|p|q|r|s|t|u|v|w|x|y|z)"
                  "((a|b|c|d|e|f|g|h|i|j|k|l|m|n|o|p|q|r|s|t|u|v|w|x|y|z)|"
                  "(A|B|C|D|E|F|G|H|I|J|K|L|M|N|O|P|Q|R|S|T|U|V|W|X|Y|Z)|"
                  "_|"
                  "(0|1|2|3|4|5|6|7|8|9))"
                  "*"},
    {UIDENT,      "(A|B|C|D|E|F|G|H|I|J|K|L|M|N|O|P|Q|R|S|T|U|V|W|X|Y|Z)"
                  "((a|b|c|d|e|f|g|h|i|j|k|l|m|n|o|p|q|r|s|t|u|v|w|x|y|z)|"
                  "(A|B|C|D|E|F|G|H|I|J|K|L|M|N|O|P|Q|R|S|T|U|V|W|X|Y|Z)|"
                  "_|"
                  "(0|1|2|3|4|5|6|7|8|9))"
                  "*"},
    {DECIMAL,     "(0|1|2|3|4|5|6|7|8|9)(0|1|2|3|4|5|6|7|8|9)*"},
    {ASSIGN,      ":="},
    {DISJUNCTION, "!!"},
    {CONJUNCTION, "&&"},
    {EQ,          "=="},
    {NEQ,         "!="},
    {LEQ,         "<="},
    {LE,          "<"},
    {GEQ,         ">="},
    {GE,          ">"},
    {PLUS,        "+"},
    {MINUS,       "-"},
    {MUL,         "\\*"},
    {DIV,         "/"},
    {MOD,         "%"},
    {SEMICOLON,   ";"},
    {COMMA,       ","},
    {LBRACE,      "\\("},
    {RBRACE,      "\\)"},
    {EOL_COMMENT,           "--"},
    {BLOCK_COMMENT_BEGIN,   "\\(\\*"},
    {BLOCK_COMMENT_END,     "\\*\\)"},
};

/* Lex a source string into a token list. */
std::vector<token> lex(const std::string &src);
