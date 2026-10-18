#pragma once

#include "grammar.h"
#include "string"
#include "vector"

struct token {
    enum terminal type;
    std::string text;
};

/* Lex a source string into a token list. */
std::vector<token> lex(const std::string &src);
