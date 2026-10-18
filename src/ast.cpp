#include "ast.h"

builder_fn table[nonterminal_max][ALT_CAP] = {
    /* EXPR */
    {
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<compare>(EQ, std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<compare>(NEQ, std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<compare>(LE, std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<compare>(GE, std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<compare>(LEQ, std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<compare>(GEQ, std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr { return std::move(ch[0]); },
    },
    /* BINEXPR */
    {
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<binexpr>(PLUS, std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<binexpr>(MINUS, std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr { return std::move(ch[0]); },
    },
    /* TERM */
    {
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<binexpr>(MUL, std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<binexpr>(DIV, std::move(ch[0]), std::move(ch[2]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr { return std::move(ch[0]); },
    },
    /* UNARY */
    {
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<unary_neg>(std::move(ch[1]));
        },
        [](std::vector<node_ptr> &ch) -> node_ptr { return std::move(ch[0]); },
    },
    /* PRIMARY */
    {
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<number_literal>(
                static_cast<token_node *>(ch[0].get())->tok);
        },
        [](std::vector<node_ptr> &ch) -> node_ptr {
            return std::make_unique<paren_expr>(std::move(ch[1]));
        },
    },
};
