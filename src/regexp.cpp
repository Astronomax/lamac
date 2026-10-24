#include "regexp.h"

#include "cassert"
#include "algorithm"
#include "memory"
#include "set"
#include "stdexcept"
#include "string"
#include "unordered_map"
#include "unordered_set"
#include "utility"
#include "vector"

namespace {

enum class ast_node_t {
    CONCAT,
    ALTER,
    KLEENE,
    LETTER,
    ANY,
    EXCEPT,
    EMPTY
};

struct ast_node {
    ast_node_t t;
    std::string s;
    std::unique_ptr<ast_node> l, r;

    ast_node(ast_node_t t, const std::string &s,
             std::unique_ptr<ast_node> l = nullptr,
             std::unique_ptr<ast_node> r = nullptr)
        : t(t), s(s), l(std::move(l)), r(std::move(r)) {}
};

// Operator priority: lower index = lower precedence = splits first.
static const char op_chars[] = {'|', '_', '*', '^'};
static const ast_node_t op_ast[] = {
    ast_node_t::ALTER, ast_node_t::CONCAT, ast_node_t::KLEENE, ast_node_t::EXCEPT};
static const int op_count = sizeof(op_chars) / sizeof(*op_chars);

struct ops_key_hash {
    size_t operator()(const std::pair<int,char> &p) const {
        return std::hash<int>()((p.first << 8) | (unsigned char)p.second);
    }
};
using ops_map = std::unordered_map<std::pair<int,char>, std::vector<int>, ops_key_hash>;

static std::unique_ptr<ast_node>
parse(const std::string &t, int l, int r,
    const std::vector<int> &bal,
    ops_map &ops,
    const std::vector<char> &is_lit)
{
    if (l >= r)
        return std::make_unique<ast_node>(ast_node_t::EMPTY, "#");
    if (l + 1 == r) {
        if (is_lit[l])
            return std::make_unique<ast_node>(ast_node_t::LETTER, std::string(1, t[l]));
        if (t[l] == '.')
            return std::make_unique<ast_node>(ast_node_t::ANY, "#");
        return nullptr;
    }

    char found_op = 0;
    int found_pos = -1;
    for (int o = 0; o < op_count; o++) {
        char op = op_chars[o];
        auto &v = ops[{bal[l], op}];
        auto it = std::lower_bound(v.begin(), v.end(), r);
        if (it != v.begin()) {
            int p = *(--it);
            if (p >= l) {
                found_op = op;
                found_pos = p;
                break;
            }
        }
    }

    if (found_op == '|') {
        auto l_ = parse(t, l, found_pos, bal, ops, is_lit);
        auto r_ = parse(t, found_pos + 1, r, bal, ops, is_lit);
        if (!l_) return r_;
        if (!r_) return l_;
        return std::make_unique<ast_node>(ast_node_t::ALTER, "#", std::move(l_), std::move(r_));
    }
    if (found_op == '_') {
        auto l_ = parse(t, l, found_pos, bal, ops, is_lit);
        auto r_ = parse(t, found_pos + 1, r, bal, ops, is_lit);
        if (!l_) return r_;
        if (!r_) return l_;
        return std::make_unique<ast_node>(ast_node_t::CONCAT, "#", std::move(l_), std::move(r_));
    }
    if (found_op == '*') {
        if (found_pos + 1 != r)
            throw std::invalid_argument("regex: '*' must appear at end of subexpression");
        auto l_ = parse(t, l, found_pos, bal, ops, is_lit);
        if (!l_)
            throw std::invalid_argument("regex: '*' applied to empty operand");
        return std::make_unique<ast_node>(ast_node_t::KLEENE, "#", std::move(l_));
    }
    if (found_op == '^') {
        if (found_pos != l)
            throw std::invalid_argument("regex: '^' must appear at start of subexpression");
        for (int i = l + 1; i < r; i++)
            if (!is_lit[i])
                throw std::invalid_argument("regex: '^' operand must consist of plain literals");
        return std::make_unique<ast_node>(ast_node_t::EXCEPT, t.substr(l + 1, r - l - 1));
    }
    if (!is_lit[l] && t[l] == '(' && !is_lit[r - 1] && t[r - 1] == ')')
        return parse(t, l + 1, r - 1, bal, ops, is_lit);
    return nullptr;
}

static std::unique_ptr<ast_node>
parse(const std::string &s)
{
    if (s.empty())
        return std::make_unique<ast_node>(ast_node_t::EMPTY, "#");

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
        } else if (c == '|' || c == '*' || c == '(' || c == ')' || c == '.' || c == '^') {
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
        return std::make_unique<ast_node>(ast_node_t::EMPTY, "#");

    /* Insert explicit concat markers between adjacent operand tokens. */
    bool except_operand = false;
    auto starts_operand = [](char ch, bool lit) {
        return lit || ch == '.' || ch == '^' || ch == '(';
    };
    auto ends_operand = [](char ch, bool lit) {
        return lit || ch == '.' || ch == '^' || ch == ')' || ch == '*';
    };
    std::string t;
    std::vector<char> is_lit;
    for (int i = 0; i < (int)raw.size(); i++) {
        if (i > 0) {
            if (except_operand) {
                if (!raw_lit[i]) {
                    if (starts_operand(raw[i], raw_lit[i])) {
                        t.push_back('_');
                        is_lit.push_back(0);
                    }
                    except_operand = false;
                }
            } else if (ends_operand(raw[i - 1], raw_lit[i - 1]) &&
                       starts_operand(raw[i], raw_lit[i])) {
                t.push_back('_');
                is_lit.push_back(0);
            }
        }
        if (raw[i] == '^' && !raw_lit[i])
            except_operand = true;
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
    if (bal.back() != 0)
        throw std::invalid_argument("regex: unbalanced parentheses");

    ops_map ops;
    for (int i = 0; i < (int)t.size(); i++) {
        if (is_lit[i]) continue;
        int b = bal[i + 1];
        for (char op : op_chars) {
            if (t[i] == op) {
                ops[{b, op}].push_back(i);
                break;
            }
        }
    }

    auto result = parse(t, 0, (int)t.size(), bal, ops, is_lit);
    if (!result)
        throw std::invalid_argument("regex: invalid expression");
    return result;
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

    if (v == nullptr || v->t == ast_node_t::EMPTY) {
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
        assert(v->s.size() == 1);
        start->edges[(unsigned char)v->s[0]].push_back(finish);
    } else if (v->t == ast_node_t::ANY) {
        for (int i = 0; i < 256; i++)
            start->edges[i].push_back(finish);
    } else if (v->t == ast_node_t::EXCEPT) {
        std::unordered_set<unsigned char> excset;
        for (unsigned char c : v->s)
            excset.insert(c);
        for (int i = 0; i < 256; i++)
            if (!excset.count((unsigned char)i))
                start->edges[i].push_back(finish);
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
