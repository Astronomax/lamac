#include "lexer.h"

#include "cctype"

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

std::vector<token> lex(const std::string &src) {
    std::vector<token> out;
    size_t i = 0;
    size_t n = src.size();

    while (i < n) {
        char c = src[i];

        /* skip whitespace */
        if (std::isspace(static_cast<unsigned char>(c))) {
            i += 1;
            continue;
        }

        size_t start = i;

        /* number literal */
        if (is_digit(c)) {
            while (i < n && is_digit(src[i]))
                i += 1;
            out.push_back({NUMBER, src.substr(start, i - start)});
            continue;
        }

        /* two-character operators */
        if (i + 1 < n) {
            char d = src[i + 1];
            if (c == '=' && d == '=') { out.push_back({EQ,  src.substr(start, 2)}); i += 2; continue; }
            if (c == '!' && d == '=') { out.push_back({NEQ, src.substr(start, 2)}); i += 2; continue; }
            if (c == '<' && d == '=') { out.push_back({LEQ, src.substr(start, 2)}); i += 2; continue; }
            if (c == '>' && d == '=') { out.push_back({GEQ, src.substr(start, 2)}); i += 2; continue; }
        }

        /* single-character operators and punctuation */
        switch (c) {
            case '<': out.push_back({LE,     src.substr(start, 1)}); i += 1; continue;
            case '>': out.push_back({GE,     src.substr(start, 1)}); i += 1; continue;
            case '+': out.push_back({PLUS,   src.substr(start, 1)}); i += 1; continue;
            case '-': out.push_back({MINUS,  src.substr(start, 1)}); i += 1; continue;
            case '*': out.push_back({MUL,    src.substr(start, 1)}); i += 1; continue;
            case '/': out.push_back({DIV,    src.substr(start, 1)}); i += 1; continue;
            case '(': out.push_back({LBRACE, src.substr(start, 1)}); i += 1; continue;
            case ')': out.push_back({RBRACE, src.substr(start, 1)}); i += 1; continue;
        }

        /* unrecognized character: skip it */
        i += 1;
    }

    return out;
}
