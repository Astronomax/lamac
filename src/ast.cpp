#include "ast.h"

builder_fn table[nonterminal_max][ALT_CAP] = {
    /* SCOPE_EXPR */ {
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<scope_expr_node>(std::move(std::get<node_ptr>(ch[0])), std::move(std::get<node_ptr>(ch[1])));
        },
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<scope_expr_node>(std::move(std::get<node_ptr>(ch[0])), nullptr);
        },
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<scope_expr_node>(nullptr, std::move(std::get<node_ptr>(ch[0])));
        },
    },
    /* EXPRESSION */ {
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<binexpr>(SEMICOLON, std::move(std::get<node_ptr>(ch[0])), std::move(std::get<node_ptr>(ch[2])));
        },
        [](std::vector<build_result> &ch) -> build_result { return std::move(ch[0]); },
    },
    /* BASIC_EXPRESSION */ {
        [](std::vector<build_result> &ch) -> build_result { return std::move(ch[0]); },
    },
    /* BINARY_EXPRESSION */ {
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<binexpr>(ASSIGN, std::move(std::get<node_ptr>(ch[0])), std::move(std::get<node_ptr>(ch[2])));
        },
        [](std::vector<build_result> &ch) -> build_result { return std::move(ch[0]); },
    },
    /* DISJUNCTION_EXPR */ {
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<binexpr>(DISJUNCTION, std::move(std::get<node_ptr>(ch[0])), std::move(std::get<node_ptr>(ch[2])));
        },
        [](std::vector<build_result> &ch) -> build_result { return std::move(ch[0]); },
    },
    /* CONJUNCTION_EXPR */ {
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<binexpr>(CONJUNCTION, std::move(std::get<node_ptr>(ch[0])), std::move(std::get<node_ptr>(ch[2])));
        },
        [](std::vector<build_result> &ch) -> build_result { return std::move(ch[0]); },
    },
    /* COMPARISON_EXPR */ {
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<binexpr>(EQ,  std::move(std::get<node_ptr>(ch[0])), std::move(std::get<node_ptr>(ch[2])));
        },
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<binexpr>(NEQ, std::move(std::get<node_ptr>(ch[0])), std::move(std::get<node_ptr>(ch[2])));
        },
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<binexpr>(LEQ, std::move(std::get<node_ptr>(ch[0])), std::move(std::get<node_ptr>(ch[2])));
        },
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<binexpr>(LE,  std::move(std::get<node_ptr>(ch[0])), std::move(std::get<node_ptr>(ch[2])));
        },
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<binexpr>(GEQ, std::move(std::get<node_ptr>(ch[0])), std::move(std::get<node_ptr>(ch[2])));
        },
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<binexpr>(GE,  std::move(std::get<node_ptr>(ch[0])), std::move(std::get<node_ptr>(ch[2])));
        },
        [](std::vector<build_result> &ch) -> build_result { return std::move(ch[0]); },
    },
    /* ADDITIVE_EXPR */ {
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<binexpr>(PLUS,  std::move(std::get<node_ptr>(ch[0])), std::move(std::get<node_ptr>(ch[2])));
        },
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<binexpr>(MINUS, std::move(std::get<node_ptr>(ch[0])), std::move(std::get<node_ptr>(ch[2])));
        },
        [](std::vector<build_result> &ch) -> build_result { return std::move(ch[0]); },
    },
    /* MULT_EXPR */ {
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<binexpr>(MUL, std::move(std::get<node_ptr>(ch[0])), std::move(std::get<node_ptr>(ch[2])));
        },
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<binexpr>(DIV, std::move(std::get<node_ptr>(ch[0])), std::move(std::get<node_ptr>(ch[2])));
        },
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<binexpr>(MOD, std::move(std::get<node_ptr>(ch[0])), std::move(std::get<node_ptr>(ch[2])));
        },
        [](std::vector<build_result> &ch) -> build_result { return std::move(ch[0]); },
    },
    /* BINARY_OPERAND */ {
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<unary_neg>(std::move(std::get<node_ptr>(ch[1])));
        },
        [](std::vector<build_result> &ch) -> build_result { return std::move(ch[0]); },
    },
    /* POSTFIX_EXPRESSION */ {
        [](std::vector<build_result> &ch) -> build_result { return std::move(ch[0]); },
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<fun_call_node>(
                std::move(std::get<node_ptr>(ch[0])), std::vector<node_ptr>{});
        },
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<fun_call_node>(
                std::move(std::get<node_ptr>(ch[0])),
                std::move(std::get<std::vector<node_ptr>>(ch[2])));
        },
    },
    /* PRIMARY */ {
        [](std::vector<build_result> &ch) -> build_result {
            const auto &text = std::get<token>(ch[0]).text;
            return std::make_unique<number_literal>(std::stoi(text));
        },
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<token_node>(std::move(std::get<token>(ch[0])));
        },
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<token_node>(std::move(std::get<token>(ch[0])));
        },
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<token_node>(std::move(std::get<token>(ch[0])));
        },
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<token_node>(std::move(std::get<token>(ch[0])));
        },
        [](std::vector<build_result> &ch) -> build_result { return std::move(ch[1]); },
        [](std::vector<build_result> &ch) -> build_result { return std::move(ch[0]); },
        [](std::vector<build_result> &ch) -> build_result { return std::move(ch[0]); },
        [](std::vector<build_result> &ch) -> build_result { return std::move(ch[0]); },
        [](std::vector<build_result> &ch) -> build_result { return std::move(ch[0]); },
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<builtin_call>(
                std::get<token>(ch[0]));
        },
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<builtin_call>(
                std::get<token>(ch[0]), std::move(std::get<node_ptr>(ch[2])));
        },
    },
    /* VAR_DEF_ITEM */ {
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<var_def_item_node>(
                std::get<token>(ch[0]));
        },
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<var_def_item_node>(
                std::get<token>(ch[0]), std::move(std::get<node_ptr>(ch[2])));
        },
    },
    /* VAR_DEF_SEQ */ {
        [](std::vector<build_result> &ch) -> build_result {
            auto seq = std::make_unique<seq_node>();
            seq->items.push_back(std::move(std::get<node_ptr>(ch[0])));
            return seq;
        },
        [](std::vector<build_result> &ch) -> build_result {
            auto *seq = static_cast<seq_node *>(std::get<node_ptr>(ch[0]).get());
            seq->items.push_back(std::move(std::get<node_ptr>(ch[2])));
            return std::move(ch[0]);
        },
    },
    /* VAR_DEF */ {
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<var_def_node>(false, std::move(std::get<node_ptr>(ch[1])));
        },
    },
    /* DEFINITION */ {
        [](std::vector<build_result> &ch) -> build_result { return std::move(ch[0]); },
        [](std::vector<build_result> &ch) -> build_result { return std::move(ch[0]); },
    },
    /* DEFINITION_LIST */ {
        [](std::vector<build_result> &ch) -> build_result {
            auto seq = std::make_unique<seq_node>();
            seq->items.push_back(std::move(std::get<node_ptr>(ch[0])));
            return seq;
        },
        [](std::vector<build_result> &ch) -> build_result {
            auto *seq = static_cast<seq_node *>(std::get<node_ptr>(ch[0]).get());
            seq->items.push_back(std::move(std::get<node_ptr>(ch[1])));
            return std::move(ch[0]);
        },
    },
    /* IF_EXPRESSION */ {
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<if_expr_node>(std::move(std::get<node_ptr>(ch[1])), std::move(std::get<node_ptr>(ch[3])), nullptr);
        },
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<if_expr_node>(std::move(std::get<node_ptr>(ch[1])), std::move(std::get<node_ptr>(ch[3])), std::move(std::get<node_ptr>(ch[4])));
        },
    },
    /* ELSE_PART */ {
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<if_expr_node>(std::move(std::get<node_ptr>(ch[1])), std::move(std::get<node_ptr>(ch[3])), nullptr);
        },
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<if_expr_node>(std::move(std::get<node_ptr>(ch[1])), std::move(std::get<node_ptr>(ch[3])), std::move(std::get<node_ptr>(ch[4])));
        },
        [](std::vector<build_result> &ch) -> build_result {
            return std::move(ch[1]); /* scope_expr_node */
        },
    },
    /* WHILE_DO_EXPRESSION */ {
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<while_do_expr_node>(std::move(std::get<node_ptr>(ch[1])), std::move(std::get<node_ptr>(ch[3])));
        },
    },
    /* DO_WHILE_EXPRESSION */ {
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<do_while_expr_node>(std::move(std::get<node_ptr>(ch[3])), std::move(std::get<node_ptr>(ch[1])));
        },
    },
    /* FOR_EXPRESSION */ {
        [](std::vector<build_result> &ch) -> build_result {
            auto body = std::make_unique<seq_node>();
            body->items.push_back(std::move(std::get<node_ptr>(ch[7])));
            body->items.push_back(std::move(std::get<node_ptr>(ch[5])));
            auto loop = std::make_unique<while_do_expr_node>(std::move(std::get<node_ptr>(ch[3])), std::move(body));
            auto seq = std::make_unique<seq_node>();
            seq->items.push_back(std::move(std::get<node_ptr>(ch[1])));
            seq->items.push_back(std::move(loop));
            return std::move(seq);
        },
    },
    /* FUNCTION_DEFINITION */ {
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<fun_definition_node>(
                std::move(std::get<token>(ch[1])), std::vector<std::string>{},
                std::move(std::get<node_ptr>(ch[4])));
        },
        [](std::vector<build_result> &ch) -> build_result {
            return std::make_unique<fun_definition_node>(
                std::move(std::get<token>(ch[1])),
                std::move(std::get<std::vector<std::string>>(ch[3])),
                std::move(std::get<node_ptr>(ch[5])));
        },
    },
    /* FUNCTION_ARGUMENTS */ {
        [](std::vector<build_result> &ch) -> build_result {
            return std::vector<std::string>{std::get<token>(ch[0]).text};
        },
        [](std::vector<build_result> &ch) -> build_result {
            auto &args = std::get<std::vector<std::string>>(ch[0]);
            args.push_back(std::move(std::get<token>(ch[2]).text));
            return std::move(args);
        },
    },
    /* FUNCTION_BODY */ {
        [](std::vector<build_result> &ch) -> build_result {
            return std::move(ch[1]); /* scope_expr_node */
        },
    },
    /* POSTFIX_EXPRESSION_AGRS_LIST */ {
        [](std::vector<build_result> &ch) -> build_result {
            std::vector<node_ptr> args;
            args.push_back(std::move(std::get<node_ptr>(ch[0])));
            return args;
        },
        [](std::vector<build_result> &ch) -> build_result {
            auto &args = std::get<std::vector<node_ptr>>(ch[0]);
            args.push_back(std::move(std::get<node_ptr>(ch[2])));
            return std::move(args);
        },
    }
};
