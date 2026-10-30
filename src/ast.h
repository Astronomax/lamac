#pragma once

#include <cstdint>

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
    int32_t num;
    number_literal(int32_t n) : num(n) {}
};

struct builtin_call : ast_node {
    token name;
    node_ptr arg;
    builtin_call(token n, node_ptr a = nullptr)
        : name(std::move(n)), arg(std::move(a)) {}
};

struct seq_node : ast_node {
    std::vector<node_ptr> items;
};

struct var_def_item_node : ast_node {
    token name;
    node_ptr init;
    var_def_item_node(token n, node_ptr i = nullptr)
        : name(std::move(n)), init(std::move(i)) {}
};

struct var_def_node : ast_node {
    bool is_public;
    node_ptr seq;
    var_def_node(bool pub, node_ptr s) : is_public(pub), seq(std::move(s)) {}
};

struct scope_expr_node : ast_node {
    node_ptr defs;
    node_ptr expr;
    scope_expr_node(node_ptr d, node_ptr e) : defs(std::move(d)), expr(std::move(e)) {}
};

using builder_fn = std::function<node_ptr(std::vector<node_ptr> &children)>;

extern builder_fn table[nonterminal_max][ALT_CAP];
