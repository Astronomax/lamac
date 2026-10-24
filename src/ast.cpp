#include "ast.h"

builder_fn table[nonterminal_max][ALT_CAP] = {
    /* SCOPE_EXPR */ {
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<scope_expr_node>(std::move(ch[0]), std::move(ch[1]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<scope_expr_node>(std::move(ch[0]), nullptr);
        },
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<scope_expr_node>(nullptr, std::move(ch[0]));
        },
    },
    /* EXPRESSION */ {
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<binexpr>(SEMICOLON, std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr { return std::move(ch[0]); },
    },
    /* BASIC_EXPRESSION */ {
        [](std::vector<node_ptr> &ch) -> node_ptr { return std::move(ch[0]); },
    },
    /* BINARY_EXPRESSION */ {
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<binexpr>(ASSIGN, std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr { return std::move(ch[0]); },
    },
    /* DISJUNCTION_EXPR */ {
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<binexpr>(DISJUNCTION, std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr { return std::move(ch[0]); },
    },
    /* CONJUNCTION_EXPR */ {
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<binexpr>(CONJUNCTION, std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr { return std::move(ch[0]); },
    },
    /* COMPARISON_EXPR */ {
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<compare>(EQ,  std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<compare>(NEQ, std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<compare>(LEQ, std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<compare>(LE,  std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<compare>(GEQ, std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<compare>(GE,  std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr { return std::move(ch[0]); },
    },
    /* ADDITIVE_EXPR */ {
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<binexpr>(PLUS,  std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<binexpr>(MINUS, std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr { return std::move(ch[0]); },
    },
    /* MULT_EXPR */ {
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<binexpr>(MUL, std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<binexpr>(DIV, std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<binexpr>(MOD, std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr { return std::move(ch[0]); },
    },
    /* BINARY_OPERAND */ {
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<unary_neg>(std::move(ch[1]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr { return std::move(ch[0]); },
    },
    /* POSTFIX_EXPRESSION */ {
        [](std::vector<node_ptr> &ch) -> node_ptr { return std::move(ch[0]); },
    },
    /* PRIMARY */ {
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<number_literal>(
                static_cast<token_node *>(ch[0].get())->tok);
        },
        [](std::vector<node_ptr> &ch) -> node_ptr { return std::move(ch[0]); },
        [](std::vector<node_ptr> &ch) -> node_ptr { return std::move(ch[0]); },
        [](std::vector<node_ptr> &ch) -> node_ptr { return std::move(ch[0]); },
        [](std::vector<node_ptr> &ch) -> node_ptr { return std::move(ch[0]); },
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<paren_expr>(std::move(ch[1]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<builtin_call>(
                static_cast<token_node *>(ch[0].get())->tok);
        },
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<builtin_call>(
                static_cast<token_node *>(ch[0].get())->tok, std::move(ch[2]));
        },
    },
    /* VAR_DEF_ITEM */ {
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<var_def_item_node>(
                static_cast<token_node *>(ch[0].get())->tok);
        },
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<var_def_item_node>(
                static_cast<token_node *>(ch[0].get())->tok, std::move(ch[2]));
        },
    },
    /* VAR_DEF_SEQ */ {
        [](std::vector<node_ptr> &ch) -> node_ptr {
            auto seq = std::make_unique<seq_node>();
            seq->items.push_back(std::move(ch[0]));
            return seq;
        },
        [](std::vector<node_ptr> &ch) -> node_ptr {
            auto *seq = static_cast<seq_node *>(ch[0].get());
            seq->items.push_back(std::move(ch[2]));
            return std::move(ch[0]);
        },
    },
    /* VAR_DEF */ {
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<var_def_node>(false, std::move(ch[1]));
        },
    },
    /* DEFINITION */ {
        [](std::vector<node_ptr> &ch) -> node_ptr { return std::move(ch[0]); },
    },
    /* DEFINITION_LIST */ {
        [](std::vector<node_ptr> &ch) -> node_ptr {
            auto seq = std::make_unique<seq_node>();
            seq->items.push_back(std::move(ch[0]));
            return seq;
        },
        [](std::vector<node_ptr> &ch) -> node_ptr {
            auto *seq = static_cast<seq_node *>(ch[0].get());
            seq->items.push_back(std::move(ch[1]));
            return std::move(ch[0]);
        },
    },
};
