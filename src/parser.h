#pragma once

#include "ast.h"
#include "lexer.h"
#include "vector"

/*
 * Parse a token list into an AST.
 * Returns nullptr if parsing fails.
 */
node_ptr parse(const std::vector<token> &tokens);
