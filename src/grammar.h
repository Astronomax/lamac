#pragma once

#include "array"
#include "cstddef"
#include "variant"

enum nonterminal {
    EXPR,
    BINEXPR,
    TERM,
    UNARY,
    PRIMARY,
    nonterminal_max
};

enum terminal {
    EQ,
    NEQ,
    LE,
    GE,
    LEQ,
    GEQ,
    PLUS,
    MINUS,
    MUL,
    DIV,
    NUMBER,
    LBRACE,
    RBRACE
};

typedef std::variant<terminal, nonterminal> symbol;

inline constexpr size_t RULE_CAP = 8;
inline constexpr size_t ALT_CAP  = 8;

struct rule {
    size_t len;
    std::array<symbol, RULE_CAP> rhs;
};

inline std::array<std::array<rule, ALT_CAP>, nonterminal_max> grammar = {{
    /* EXPR */ {{
        rule{3, { EXPR, EQ,    BINEXPR }},
        rule{3, { EXPR, NEQ,   BINEXPR }},
        rule{3, { EXPR, LE,    BINEXPR }},
        rule{3, { EXPR, GE,    BINEXPR }},
        rule{3, { EXPR, LEQ,   BINEXPR }},
        rule{3, { EXPR, GEQ,   BINEXPR }},
        rule{1, { BINEXPR }},
    }},
    /* BINEXPR */ {{
        rule{3, { BINEXPR, PLUS,  TERM }},
        rule{3, { BINEXPR, MINUS, TERM }},
        rule{1, { TERM }},
    }},
    /* TERM */ {{
        rule{3, { TERM, MUL, UNARY }},
        rule{3, { TERM, DIV, UNARY }},
        rule{1, { UNARY }},
    }},
    /* UNARY */ {{
        rule{2, { MINUS, UNARY }},
        rule{1, { PRIMARY }},
    }},
    /* PRIMARY */ {{
        rule{1, { NUMBER }},
        rule{3, { LBRACE, EXPR, RBRACE }},
    }}
}};
