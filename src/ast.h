#pragma once

#include "functional"
#include "memory"
#include "utility"
#include "vector"

#include "grammar.h"
#include "lexer.h"

struct ast_node {
    virtual ~ast_node() = default;
};
using node_ptr = std::unique_ptr<ast_node>;

struct token_node : ast_node {
    token tok;
    token_node(token t) : tok(std::move(t)) {}
};

struct binexpr : ast_node {
    terminal op;
    node_ptr lhs;
    node_ptr rhs;
    binexpr(terminal op, node_ptr l, node_ptr r)
        : op(op), lhs(std::move(l)), rhs(std::move(r)) {}
};

struct unary_neg : ast_node {
    node_ptr operand;
    unary_neg(node_ptr o) : operand(std::move(o)) {}
};

struct number_literal : ast_node {
    token num;
    number_literal(token t) : num(std::move(t)) {}
};

struct paren_expr : ast_node {
    node_ptr inner;
    paren_expr(node_ptr e) : inner(std::move(e)) {}
};

struct compare : ast_node {
    terminal op;
    node_ptr lhs;
    node_ptr rhs;
    compare(terminal op, node_ptr l, node_ptr r)
        : op(op), lhs(std::move(l)), rhs(std::move(r)) {}
};

using builder_fn = std::function<node_ptr(std::vector<node_ptr> &children)>;

extern builder_fn table[nonterminal_max][ALT_CAP];
