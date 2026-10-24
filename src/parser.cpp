#include "ast.h"
#include "grammar.h"
#include "lexer.h"

#include "algorithm"
#include "array"
#include "cassert"
#include "cstddef"
#include "utility"
#include "vector"

template <typename T, size_t Rank>
struct mdarray {
    static_assert(Rank > 0, "Rank must be positive");

    T* data;
    std::array<size_t, Rank> dims;
    size_t total;

    mdarray(std::array<size_t, Rank> dims, T fill = T{}) : dims(dims) {
        total = 1;
        for (size_t i = 0; i < Rank; ++i)
            total *= dims[i];
        data = new T[total];
        std::fill(data, data + total, fill);
    }
    ~mdarray() {
        delete[] data;
    }
    mdarray(const mdarray&)            = delete;
    mdarray& operator=(const mdarray&) = delete;
    mdarray(mdarray&& o) noexcept
        : data(o.data), dims(o.dims), total(o.total) {
        o.data  = nullptr;
        o.total = 0;
    }
    mdarray& operator=(mdarray&&) = delete;

    /* flat index from an index tuple */
    size_t flat(const std::array<size_t, Rank>& idx) const {
        size_t f = 0;
        for (size_t i = 0; i < Rank; ++i)
            f = f * dims[i] + idx[i];
        return f;
    }

    /* access by a tuple/array of indices */
    T& operator()(const std::array<size_t, Rank>& idx) {
        return data[flat(idx)];
    }

    T operator()(const std::array<size_t, Rank>& idx) const {
        return data[flat(idx)];
    }

    /* access by Rank separate index arguments */
    template <typename... Idx>
    T& operator()(Idx... idx) {
        static_assert(sizeof...(Idx) == Rank, "wrong number of indices");
        return data[flat({ static_cast<size_t>(idx)... })];
    }

    template <typename... Idx>
    T operator()(Idx... idx) const {
        static_assert(sizeof...(Idx) == Rank, "wrong number of indices");
        return data[flat({ static_cast<size_t>(idx)... })];
    }
};

node_ptr
reconstruct(int i, int j, nonterminal nt1, size_t alt1,
            const std::vector<token> &tokens,
            const mdarray<int, 5> &dp, const mdarray<int, 3> &dp_agg)
{
    const rule &r = grammar[nt1][alt1];
    size_t len = r.len;
    std::vector<node_ptr> children(len);
    for (int m = len - 1; m >= 0; m--) {
        int k = dp(i, j, nt1, alt1, m);
        int pos = len - 1 - m;
        symbol sym = r.rhs[pos];
        if (std::holds_alternative<terminal>(sym)) {
            assert(i + 1 == k);
            children[pos] = std::make_unique<token_node>(tokens[i]);
        } else {
            nonterminal nt2 = std::get<nonterminal>(sym);
            int alt2 = dp_agg(i, k, nt2);
            assert(alt2 != -1);
            children[pos] = reconstruct(i, k, nt2, alt2, tokens, dp, dp_agg);
        }
        i = k;
    }
    assert(i == j);
    return table[nt1][alt1](children);
}

node_ptr
parse(const std::vector<token> &tokens)
{
    int n = tokens.size();
    /*
     * i, j - range bounds of the input,    0, ..., N
     * nt   - nonterminal,                  0, ..., nonterminal_max
     * alt  - alternative index,            0, ..., ALT_CAP
     * m    - suffix position in the rule,  0, ..., RULE_CAP
     */
    mdarray<int, 5> dp({n + 1, n + 1, nonterminal_max, ALT_CAP, RULE_CAP}, -1);
    mdarray<int, 3> dp_agg({n + 1, n + 1, nonterminal_max}, -1);

    for (int len = 1; len <= n; len++) {
        for (int i = 0; i + len <= n; i++) {
            int j = i + len;
            /*
             * Unit productions (A -> B) link nonterminals on the same segment,
             * so a single pass in enum order is not enough. Repeat until dp_agg
             * stops growing.
             */
            bool changed = true;
            while (changed) {
                changed = false;
                /* iterate over rules */
                for (int nt1 = 0; nt1 < nonterminal_max; nt1++) {
                    for (int alt = 0; alt < (int)grammar[nt1].size(); alt++) {
                        int len1 = grammar[nt1][alt].len;
                        if (len1 == 0)
                            continue; /* empty slot */
                        for (int m = 0; m < len1; m++) {
                            auto symbol = grammar[nt1][alt].rhs[len1 - m - 1];

                            if (m == 0) {
                                if (std::holds_alternative<terminal>(symbol)) {
                                    terminal t = std::get<terminal>(symbol);
                                    if (tokens[i].type == t && i + 1 == j)
                                        dp(i, j, nt1, alt, m) = j;
                                } else {
                                    nonterminal nt2 = std::get<nonterminal>(symbol);
                                    if (dp_agg(i, j, nt2) != -1)
                                        dp(i, j, nt1, alt, m) = j;
                                }
                                continue;
                            }

                            if (std::holds_alternative<terminal>(symbol)) {
                                terminal t = std::get<terminal>(symbol);
                                if (tokens[i].type == t &&
                                    dp(i + 1, j, nt1, alt, m - 1) != -1)
                                    dp(i, j, nt1, alt, m) = i + 1;
                            } else {
                                nonterminal nt2 = std::get<nonterminal>(symbol);
                                /* iterate over split position */
                                for (int k = i; k <= j; k++) {
                                    if (dp_agg(i, k, nt2) != -1 &&
                                        dp(k, j, nt1, alt, m - 1) != -1) {
                                        dp(i, j, nt1, alt, m) = k;
                                        break;
                                    }
                                }
                            }
                        }
                        if (dp_agg(i, j, nt1) == -1 &&
                            dp(i, j, nt1, alt, len1 - 1) != -1) {
                            dp_agg(i, j, nt1) = alt;
                            changed = true;
                        }
                    }
                }
            }
        }
    }
    int alt = dp_agg(0, n, SCOPE_EXPR);
    if (alt == -1)
        return nullptr;
    return reconstruct(0, n, SCOPE_EXPR, alt, tokens, dp, dp_agg);
}
