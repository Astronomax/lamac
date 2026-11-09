#pragma once

#include <cstdint>
#include "functional"
#include "memory"
#include "utility"
#include "variant"
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

struct fun_call_node : ast_node {
    node_ptr callee;
    std::vector<node_ptr> args;
    fun_call_node(node_ptr c, std::vector<node_ptr> a) :
        callee(std::move(c)), args(std::move(a)) {}
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

struct if_expr_node : ast_node {
    node_ptr condition; /* expression */
    node_ptr then_branch; /* scope_expr_node */
    node_ptr else_branch; /* nullptr | if_expr_node | scope_expr_node */
    if_expr_node(node_ptr c, node_ptr t, node_ptr e) :
        condition(std::move(c)), then_branch(std::move(t)), else_branch(std::move(e)) {}
};

struct while_do_expr_node : ast_node {
    node_ptr condition; /* expression */
    node_ptr body; /* scope_expr_node */
    while_do_expr_node(node_ptr c, node_ptr b) :
        condition(std::move(c)), body(std::move(b)) {}
};

struct do_while_expr_node : ast_node {
    node_ptr condition; /* expression */
    node_ptr body; /* scope_expr_node */
    do_while_expr_node(node_ptr c, node_ptr b) :
        condition(std::move(c)), body(std::move(b)) {}
};

struct fun_definition_node : ast_node {
    token name;
    std::vector<std::string> args; /* lidents */
    node_ptr body; /* scope_expr_node */
    fun_definition_node(token n, std::vector<std::string> a, node_ptr b) :
        name(std::move(n)), args(std::move(a)), body(std::move(b)) {}
};

using build_result = std::variant<
    node_ptr,
    token,
    std::vector<std::string>, /* function arguments definition */
    std::vector<node_ptr> /* expression* (function call arguments) */
>;

using builder_fn = std::function<build_result(std::vector<build_result> &children)>;

extern builder_fn table[nonterminal_max][ALT_CAP];
