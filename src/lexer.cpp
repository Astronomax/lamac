#include "lexer.h"

#include "cassert"
#include "cctype"
#include "regexp.h"

std::vector<token>
lex(const std::string &src)
{
    std::vector<token> tokens;
    size_t pos = 0;
    size_t n = src.size();

    bool eol_comment = false;
    bool block_comment = false;

    while (pos < n) {
        const char c = src[pos];
        if (eol_comment) {
            if (c == '\n')
                eol_comment = false;
            pos += 1;
            continue;
        }
        /* Skip whitespace. */
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
            pos += 1;
            continue;
        }
        /* Pick the spec with the longest match at pos. */
        const token_spec *best = nullptr;
        int best_len = 0;
        for (const token_spec &s : token_specs) {
            int len = match(s.regex, src, pos);
            if (len > best_len) {
                best_len = len;
                best = &s;
            }
        }
        if (best == nullptr) {
            /* Unrecognized character: skip it. TODO: fail. */
            pos += 1;
            continue;
        }
        if (block_comment) {
            if (best->type == BLOCK_COMMENT_END)
                block_comment = false;
            pos += 1;
            continue;
        }
        pos += best_len;
        if (best->type == EOL_COMMENT) {
            assert(!eol_comment && !block_comment);
            eol_comment = true;
            continue;
        }
        if (best->type == BLOCK_COMMENT_BEGIN) {
            assert(!eol_comment && !block_comment);
            block_comment = true;
            continue;
        }
        tokens.push_back({best->type, src.substr(pos - best_len, best_len)});
    }
    return tokens;
}
