#include "ast.h"
#include "grammar.h"
#include "lexer.h"
#include "parser.h"

#include "iostream"

static const char *term_name(enum terminal t) {
    switch (t) {
        case VAR:        return "var";
        case READ:       return "read";
        case WRITE:      return "write";
        case SKIP:       return "skip";
        case TRUE:       return "true";
        case FALSE:      return "false";
        case LIDENT:     return "lident";
        case UIDENT:     return "uident";
        case DECIMAL:    return "decimal";
        case ASSIGN:     return ":=";
        case DISJUNCTION:return "!!";
        case CONJUNCTION:return "&&";
        case EQ:         return "==";
        case NEQ:        return "!=";
        case LEQ:        return "<=";
        case LE:         return "<";
        case GEQ:        return ">=";
        case GE:         return ">";
        case PLUS:       return "+";
        case MINUS:      return "-";
        case MUL:        return "*";
        case DIV:        return "/";
        case MOD:        return "%";
        case SEMICOLON:  return ";";
        case COMMA:      return ",";
        case LBRACE:     return "(";
        case RBRACE:     return ")";
        default:         return "?";
    }
}

static void print_tokens(const std::vector<token> &tokens) {
    std::cout << "tokens:";
    for (const auto &t : tokens)
        std::cout << " " << term_name(t.type);
    std::cout << "\n";
}

static void dump(const node_ptr &node, int depth) {
    for (int i = 0; i < depth; i++)
        std::cout << "  ";

    if (auto p = dynamic_cast<token_node *>(node.get())) {
        std::cout << "token:" << term_name(p->tok.type) << "\n";
        return;
    }
    if (auto p = dynamic_cast<number_literal *>(node.get())) {
        std::cout << "number:" << p->num.text << "\n";
        return;
    }
    if (auto p = dynamic_cast<binexpr *>(node.get())) {
        std::cout << "binexpr:" << term_name(p->op) << "\n";
        dump(p->lhs, depth + 1);
        dump(p->rhs, depth + 1);
        return;
    }
    if (auto p = dynamic_cast<compare *>(node.get())) {
        std::cout << "compare:" << term_name(p->op) << "\n";
        dump(p->lhs, depth + 1);
        dump(p->rhs, depth + 1);
        return;
    }
    if (auto p = dynamic_cast<unary_neg *>(node.get())) {
        std::cout << "unary_neg\n";
        dump(p->operand, depth + 1);
        return;
    }
    if (auto p = dynamic_cast<paren_expr *>(node.get())) {
        std::cout << "paren\n";
        dump(p->inner, depth + 1);
        return;
    }
    if (auto p = dynamic_cast<builtin_call *>(node.get())) {
        std::cout << "builtin:" << term_name(p->name.type) << "\n";
        if (p->arg) dump(p->arg, depth + 1);
        return;
    }
    if (auto p = dynamic_cast<var_def_item_node *>(node.get())) {
        std::cout << "var_def_item:" << p->name.text << "\n";
        if (p->init) dump(p->init, depth + 1);
        return;
    }
    if (auto p = dynamic_cast<var_def_node *>(node.get())) {
        std::cout << "var_def\n";
        dump(p->seq, depth + 1);
        return;
    }
    if (auto p = dynamic_cast<scope_expr_node *>(node.get())) {
        std::cout << "scope\n";
        if (p->defs) dump(p->defs, depth + 1);
        if (p->expr) dump(p->expr, depth + 1);
        return;
    }
    if (auto p = dynamic_cast<seq_node *>(node.get())) {
        std::cout << "seq\n";
        for (auto &item : p->items) dump(item, depth + 1);
        return;
    }
    std::cout << "<unknown node>\n";
}

int main(int argc, char **argv) {
    std::string src = (argc > 1) ? argv[1] : "1 + 2 * 3 == 4";
    std::vector<token> tokens = lex(src);
    print_tokens(tokens);

    node_ptr ast = parse(tokens);
    if (!ast) {
        std::cout << "parse: no derivation\n";
        return 1;
    }
    dump(ast, 0);
    return 0;
}
