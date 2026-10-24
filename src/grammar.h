#pragma once

#include "array"
#include "cstddef"
#include "variant"

enum nonterminal {
    SCOPE_EXPR,
    EXPRESSION,
    BASIC_EXPRESSION,
    BINARY_EXPRESSION,
    DISJUNCTION_EXPR,
    CONJUNCTION_EXPR,
    COMPARISON_EXPR,
    ADDITIVE_EXPR,
    MULT_EXPR,
    BINARY_OPERAND,
    POSTFIX_EXPRESSION,
    PRIMARY,
    VAR_DEF_ITEM,
    VAR_DEF_SEQ,
    VAR_DEF,
    DEFINITION,
    DEFINITION_LIST,
    nonterminal_max
};

enum terminal {
    VAR,
    READ,
    WRITE,
    SKIP,
    TRUE,
    FALSE,
    LIDENT,
    UIDENT,
    DECIMAL,
    ASSIGN,
    DISJUNCTION,
    CONJUNCTION,
    EQ,
    NEQ,
    LEQ,
    LE,
    GEQ,
    GE,
    PLUS,
    MINUS,
    MUL,
    DIV,
    MOD,
    SEMICOLON,
    COMMA,
    LBRACE,
    RBRACE,
    EOL_COMMENT,
    BLOCK_COMMENT_BEGIN,
    BLOCK_COMMENT_END
};

typedef std::variant<terminal, nonterminal> symbol;

inline constexpr size_t RULE_CAP = 4;
inline constexpr size_t ALT_CAP  = 8;

struct rule {
    size_t len;
    std::array<symbol, RULE_CAP> rhs;
};

inline std::array<std::array<rule, ALT_CAP>, nonterminal_max> grammar = {{
    /* SCOPE_EXPR : definition* expression | definition* | expression */ {{
        rule{2, { DEFINITION_LIST, EXPRESSION }},
        rule{1, { DEFINITION_LIST }},
        rule{1, { EXPRESSION }},
    }},
    /* EXPRESSION : basicExpression ';' expression | basicExpression */ {{
        rule{3, { BASIC_EXPRESSION, SEMICOLON, EXPRESSION }},
        rule{1, { BASIC_EXPRESSION }},
    }},
    /* BASIC_EXPRESSION : binaryExpression */ {{
        rule{1, { BINARY_EXPRESSION }},
    }},
    /* BINARY_EXPRESSION : disjunctionExpr ':=' binaryExpression (right-assoc) | disjunctionExpr */ {{
        rule{3, { DISJUNCTION_EXPR, ASSIGN, BINARY_EXPRESSION }},
        rule{1, { DISJUNCTION_EXPR }},
    }},
    /* DISJUNCTION_EXPR : disjunctionExpr '!!' conjunctionExpr (left-assoc) | conjunctionExpr */ {{
        rule{3, { DISJUNCTION_EXPR, DISJUNCTION, CONJUNCTION_EXPR }},
        rule{1, { CONJUNCTION_EXPR }},
    }},
    /* CONJUNCTION_EXPR : conjunctionExpr '&&' comparisonExpr (left-assoc) | comparisonExpr */ {{
        rule{3, { CONJUNCTION_EXPR, CONJUNCTION, COMPARISON_EXPR }},
        rule{1, { COMPARISON_EXPR }},
    }},
    /* COMPARISON_EXPR : additiveExpr ('=='|'!='|'<='|'<'|'>='|'>') additiveExpr (non-assoc) | additiveExpr */ {{
        rule{3, { ADDITIVE_EXPR, EQ,  ADDITIVE_EXPR }},
        rule{3, { ADDITIVE_EXPR, NEQ, ADDITIVE_EXPR }},
        rule{3, { ADDITIVE_EXPR, LEQ, ADDITIVE_EXPR }},
        rule{3, { ADDITIVE_EXPR, LE,  ADDITIVE_EXPR }},
        rule{3, { ADDITIVE_EXPR, GEQ, ADDITIVE_EXPR }},
        rule{3, { ADDITIVE_EXPR, GE,  ADDITIVE_EXPR }},
        rule{1, { ADDITIVE_EXPR }},
    }},
    /* ADDITIVE_EXPR : additiveExpr ('+'|'-') multExpr (left-assoc) | multExpr */ {{
        rule{3, { ADDITIVE_EXPR, PLUS,  MULT_EXPR }},
        rule{3, { ADDITIVE_EXPR, MINUS, MULT_EXPR }},
        rule{1, { MULT_EXPR }},
    }},
    /* MULT_EXPR : multExpr ('*'|'/'|'%') binaryOperand (left-assoc) | binaryOperand */ {{
        rule{3, { MULT_EXPR, MUL, BINARY_OPERAND }},
        rule{3, { MULT_EXPR, DIV, BINARY_OPERAND }},
        rule{3, { MULT_EXPR, MOD, BINARY_OPERAND }},
        rule{1, { BINARY_OPERAND }},
    }},
    /* BINARY_OPERAND : '-' postfixExpression | postfixExpression */ {{
        rule{2, { MINUS, POSTFIX_EXPRESSION }},
        rule{1, { POSTFIX_EXPRESSION }},
    }},
    /* POSTFIX_EXPRESSION : primary */ {{
        rule{1, { PRIMARY }},
    }},
    /* PRIMARY : DECIMAL | LIDENT | 'true' | 'false' | 'skip'
              | '(' scopeExpression ')'
              | 'read' '(' ')'
              | 'write' '(' expression ')' */ {{
        rule{1, { DECIMAL }},
        rule{1, { LIDENT }},
        rule{1, { TRUE }},
        rule{1, { FALSE }},
        rule{1, { SKIP }},
        rule{3, { LBRACE, SCOPE_EXPR, RBRACE }},
        rule{3, { READ,   LBRACE,     RBRACE }},
        rule{4, { WRITE,  LBRACE,     EXPRESSION, RBRACE }},
    }},
    /* VAR_DEF_ITEM : LIDENT | LIDENT ':=' basicExpression */ {{
        rule{1, { LIDENT }},
        rule{3, { LIDENT, ASSIGN, BASIC_EXPRESSION }},
    }},
    /* VAR_DEF_SEQ : variableDefinitionItem | variableDefinitionSequence ',' variableDefinitionItem */ {{
        rule{1, { VAR_DEF_ITEM }},
        rule{3, { VAR_DEF_SEQ, COMMA, VAR_DEF_ITEM }},
    }},
    /* VAR_DEF : 'var' variableDefinitionSequence ';' */ {{
        rule{3, { VAR, VAR_DEF_SEQ, SEMICOLON }},
    }},
    /* DEFINITION : variableDefinition */ {{
        rule{1, { VAR_DEF }},
    }},
    /* DEFINITION_LIST : definition | definition* definition */ {{
        rule{1, { DEFINITION }},
        rule{2, { DEFINITION_LIST, DEFINITION }},
    }},
}};
