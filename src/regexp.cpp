#include "regexp.h"

#include "algorithm"
#include "memory"
#include "set"
#include "string"
#include "unordered_map"
#include "utility"
#include "vector"

namespace {

enum class ast_node_t {
    CONCAT,
    ALTER,
    KLEENE,
    LETTER,
    ANY,
    EMPTY
};

struct ast_node {
    ast_node_t t;
    char c;
    std::unique_ptr<ast_node> l, r;

    ast_node(ast_node_t t, char c,
             std::unique_ptr<ast_node> l = nullptr,
             std::unique_ptr<ast_node> r = nullptr)
        : t(t), c(c), l(std::move(l)), r(std::move(r)) {}
};

static std::unique_ptr<ast_node>
parse(const std::string &t, int l, int r,
    const std::vector<int> &bal,
    std::unordered_map<int, std::vector<int>> &alt,
    std::unordered_map<int, std::vector<int>> &con,
    std::unordered_map<int, std::vector<int>> &kle,
    const std::vector<char> &is_lit)
{
    if (l >= r) {
        return std::make_unique<ast_node>(ast_node_t::EMPTY, '#');
    } else if (l + 1 == r) {
        if (is_lit[l])
            return std::make_unique<ast_node>(ast_node_t::LETTER, t[l]);
        else if (t[l] == '.')
            return std::make_unique<ast_node>(ast_node_t::ANY, '#');
        else
            return nullptr;
    } else {
        int _con = -1, _alt = -1, _kle = -1;
        auto it = std::lower_bound(con[bal[l]].begin(), con[bal[l]].end(), r);
        if (it != con[bal[l]].begin())
            _con = *(--it);
        if (_con < l)
            _con = -1;
        it = std::lower_bound(alt[bal[l]].begin(), alt[bal[l]].end(), r);
        if (it != alt[bal[l]].begin())
            _alt = *(--it);
        if (_alt < l)
            _alt = -1;
        it = std::lower_bound(kle[bal[l]].begin(), kle[bal[l]].end(), r);
        if (it != kle[bal[l]].begin())
            _kle = *(--it);
        if (_kle < l)
            _kle = -1;

        if (_alt != -1) {
            auto l_ = parse(t, l, _alt, bal, alt, con, kle, is_lit);
            auto r_ = parse(t, _alt + 1, r, bal, alt, con, kle, is_lit);
            if (!l_)
                return r_;
            if (!r_)
                return l_;
            return std::make_unique<ast_node>(ast_node_t::ALTER, '#', std::move(l_), std::move(r_));
        } else if (_con != -1) {
            auto l_ = parse(t, l, _con, bal, alt, con, kle, is_lit);
            auto r_ = parse(t, _con + 1, r, bal, alt, con, kle, is_lit);
            if (!l_)
                return r_;
            if (!r_)
                return l_;
            return std::make_unique<ast_node>(ast_node_t::CONCAT, '#', std::move(l_), std::move(r_));
        } else if (_kle != -1) {
            auto l_ = parse(t, l, _kle, bal, alt, con, kle, is_lit);
            if (!l_)
                return nullptr;
            return std::make_unique<ast_node>(ast_node_t::KLEENE, '#', std::move(l_));
        } else if (!is_lit[l] && t[l] == '(' && !is_lit[r - 1] && t[r - 1] == ')') {
            return parse(t, l + 1, r - 1, bal, alt, con, kle, is_lit);
        } else {
            return nullptr;
        }
    }
}

static std::unique_ptr<ast_node>
parse(const std::string &s)
{
    if (s.empty())
        return std::make_unique<ast_node>(ast_node_t::EMPTY, '#');

    /*
     * Tokenize: collapse \x escapes into a single literal token, and classify
     * each token as literal (is_lit=1) or operator/wildcard (is_lit=0).
     */
    std::string raw;
    std::vector<char> raw_lit;
    for (int i = 0; i < (int)s.size();) {
        char c = s[i];
        if (c == '\\') {
            if (i + 1 < (int)s.size()) {
                raw.push_back(s[i + 1]);
                raw_lit.push_back(1);
                i += 2;
            } else {
                i += 1;
            }
        } else if (c == '|' || c == '*' || c == '(' || c == ')' || c == '.') {
            raw.push_back(c);
            raw_lit.push_back(0);
            i += 1;
        } else {
            raw.push_back(c);
            raw_lit.push_back(1);
            i += 1;
        }
    }
    if (raw.empty())
        return std::make_unique<ast_node>(ast_node_t::EMPTY, '#');

    /* Insert explicit concat markers between adjacent operand tokens. */
    auto starts_operand = [](char ch, bool lit) {
        return lit || ch == '.' || ch == '(';
    };
    auto ends_operand = [](char ch, bool lit) {
        return lit || ch == '.' || ch == ')' || ch == '*';
    };
    std::string t;
    std::vector<char> is_lit;
    for (int i = 0; i < (int)raw.size(); i++) {
        if (i > 0 && ends_operand(raw[i - 1], raw_lit[i - 1])
                  && starts_operand(raw[i], raw_lit[i])) {
            t.push_back('_');
            is_lit.push_back(0);
        }
        t.push_back(raw[i]);
        is_lit.push_back(raw_lit[i]);
    }

    std::vector<int> bal(t.size() + 1);
    for (int i = 0; i < (int)t.size(); i++) {
        bal[i + 1] = bal[i];
        if (!is_lit[i]) {
            if (t[i] == '(') bal[i + 1]++;
            else if (t[i] == ')') bal[i + 1]--;
        }
    }
    std::unordered_map<int, std::vector<int>> alt, con, kle;
    for (int i = 0; i < (int)t.size(); i++) {
        if (is_lit[i])
            continue;
        if (t[i] == '|')
            alt[bal[i + 1]].push_back(i);
        else if (t[i] == '_')
            con[bal[i + 1]].push_back(i);
        else if (t[i] == '*')
            kle[bal[i + 1]].push_back(i);
    }
    return parse(t, 0, t.size(), bal, alt, con, kle, is_lit);
}

struct nfa_node {
    std::vector<nfa_node *> edges[256];
    std::vector<nfa_node *> eps;
};

struct nfa {
    std::vector<std::unique_ptr<nfa_node>> nodes;
    nfa_node *start  = nullptr;
    nfa_node *finish = nullptr;
};

static std::pair<nfa_node *, nfa_node *>
nfa_from_ast(const ast_node *v, nfa *d)
{
    d->nodes.push_back(std::make_unique<nfa_node>());
    nfa_node *start = d->nodes.back().get();
    d->nodes.push_back(std::make_unique<nfa_node>());
    nfa_node *finish = d->nodes.back().get();

    if (v == nullptr) {
        start->eps.push_back(finish);
    } else if (v->t == ast_node_t::CONCAT) {
        auto [ls, lf] = nfa_from_ast(v->l.get(), d);
        auto [rs, rf] = nfa_from_ast(v->r.get(), d);
        start->eps.push_back(ls);
        lf->eps.push_back(rs);
        rf->eps.push_back(finish);
    } else if (v->t == ast_node_t::ALTER) {
        auto [ls, lf] = nfa_from_ast(v->l.get(), d);
        auto [rs, rf] = nfa_from_ast(v->r.get(), d);
        start->eps.push_back(ls);
        start->eps.push_back(rs);
        lf->eps.push_back(finish);
        rf->eps.push_back(finish);
    } else if (v->t == ast_node_t::KLEENE) {
        auto [ls, lf] = nfa_from_ast(v->l.get(), d);
        start->eps.push_back(ls);
        lf->eps.push_back(finish);
        start->eps.push_back(finish);
        lf->eps.push_back(ls);
    } else if (v->t == ast_node_t::LETTER) {
        start->edges[(unsigned char)v->c].push_back(finish);
    } else if (v->t == ast_node_t::ANY) {
        for (int i = 0; i < 256; i++)
            start->edges[i].push_back(finish);
    } else if (v->t == ast_node_t::EMPTY) {
        start->eps.push_back(finish);
    }
    return {start, finish};
}

static std::unique_ptr<nfa>
nfa_from_regex(const std::string &regex)
{
    auto ast = parse(regex);
    auto d = std::make_unique<nfa>();
    auto [start, finish] = nfa_from_ast(ast.get(), d.get());
    d->start = start;
    d->finish = finish;
    return d;
}

/* All states reachable from s via eps edges only. */
static std::vector<nfa_node *>
eps_closure(const std::vector<nfa_node *> &s)
{
    std::vector<nfa_node *> stack(s.begin(), s.end());
    std::vector<nfa_node *> out;
    std::set<nfa_node *> seen;
    while (!stack.empty()) {
        nfa_node *v = stack.back();
        stack.pop_back();
        if (!seen.insert(v).second)
            continue;
        out.push_back(v);
        for (nfa_node *e : v->eps)
            if (!seen.count(e))
                stack.push_back(e);
    }
    return out;
}

static int
nfa_match(const nfa &d, const std::string &src, size_t pos)
{
    std::vector<nfa_node *> cur = eps_closure({d.start});
    int last_accept = -1;

    auto contains_finish = [&](const std::vector<nfa_node *> &s) {
        for (nfa_node *v : s)
            if (v == d.finish)
                return true;
        return false;
    };

    if (contains_finish(cur))
        last_accept = 0;

    size_t len = 0;
    while (pos + len < src.size()) {
        unsigned char c = static_cast<unsigned char>(src[pos + len]);
        std::vector<nfa_node *> next;
        std::set<nfa_node *> seen;
        for (nfa_node *v : cur)
            for (nfa_node *e : v->edges[c])
                if (seen.insert(e).second)
                    next.push_back(e);
        cur = eps_closure(next);
        if (cur.empty())
            break;
        len += 1;
        if (contains_finish(cur))
            last_accept = len;
    }
    return last_accept;
}

} /* namespace */

int
match(const std::string &regex, const std::string &src, size_t pos)
{
    /* Cache compiled NFAs by regex string. */
    static std::unordered_map<std::string, std::unique_ptr<nfa>> cache;
    auto &ent = cache[regex];
    if (!ent)
        ent = nfa_from_regex(regex);
    return nfa_match(*ent, src, pos);
}
