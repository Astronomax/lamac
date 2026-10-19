#include "lexer.h"

#include "cctype"
#include "regexp.h"

std::vector<token>
lex(const std::string &src)
{
    std::vector<token> tokens;
    size_t pos = 0;
    size_t n = src.size();

    while (pos < n) {
        /* Skip whitespace. */
        if (std::isspace(static_cast<unsigned char>(src[pos]))) {
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
        tokens.push_back({best->type, src.substr(pos, best_len)});
        pos += best_len;
    }
    return tokens;
}
