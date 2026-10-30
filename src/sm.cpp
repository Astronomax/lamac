#include "sm.h"

#include <stdexcept>
#include <unordered_map>

namespace {

struct scope {
    std::unordered_map<std::string, sm::location> vars;
};

struct env {
    std::vector<scope> scopes;

    sm::location
    lookupName(const std::string &name) const;
};

sm::location
env::lookupName(const std::string &name) const
{
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->vars.find(name);
        if (found != it->vars.end())
            return found->second;
    }
    throw std::runtime_error("undefined variable: " + name);
}

sm::binary_op
binary_operator(terminal op)
{
    switch (op) {
        case PLUS: return sm::binary_op::plus;
        case MINUS: return sm::binary_op::minus;
        case MUL: return sm::binary_op::mul;
        case DIV: return sm::binary_op::div;
        case MOD: return sm::binary_op::mod;
        case LE: return sm::binary_op::le;
        case LEQ: return sm::binary_op::leq;
        case GE: return sm::binary_op::ge;
        case GEQ: return sm::binary_op::geq;
        case EQ: return sm::binary_op::eq;
        case NEQ: return sm::binary_op::neq;
        case CONJUNCTION: return sm::binary_op::conjunction;
        case DISJUNCTION: return sm::binary_op::disjunction;
        default:
            throw std::runtime_error("unsupported binary operator");
    }
}

struct compiler {
    sm::program program;
    env environment;

    bool
    emit(const ast_node &node);

    void
    emit_value(const ast_node &node);

    void
    emit_discard(const ast_node &node);

    void
    declare(const ast_node &node);
};

void
compiler::emit_value(const ast_node &node)
{
    if (!emit(node))
        throw std::runtime_error("value expected");
}

void
compiler::emit_discard(const ast_node &node)
{
    if (emit(node))
        program.code.emplace_back(sm::drop{});
}

void
compiler::declare(const ast_node &node)
{
    if (auto p = dynamic_cast<const var_def_item_node *>(&node)) {
        bool global = environment.scopes.size() == 1;
        auto &count = global ? program.global_count : program.local_count;
        auto kind = global ? sm::location_kind::global : sm::location_kind::local;
        auto &vars = environment.scopes.back().vars;
        if (!vars.emplace(p->name.text, sm::location{kind, count}).second)
            throw std::runtime_error("duplicate variable: " + p->name.text);
        ++count;
    } else if (auto p = dynamic_cast<const var_def_node *>(&node)) {
        /* ignore is_public for now */
        declare(*p->seq);
    } else if (auto p = dynamic_cast<const seq_node *>(&node)) {
        for (const auto &item : p->items)
            declare(*item);
    } else {
        throw std::runtime_error("variable definition expected");
    }
}

bool
compiler::emit(const ast_node &node)
{
    if (auto p = dynamic_cast<const token_node *>(&node)) {
        switch (p->tok.type) {
            case LIDENT:
                program.code.emplace_back(sm::load{environment.lookupName(p->tok.text)});
                return true;
            case TRUE:
                program.code.emplace_back(sm::constant{1});
                return true;
            case FALSE:
                program.code.emplace_back(sm::constant{0});
                return true;
            case SKIP:
                return false;
            default:
                throw std::runtime_error("unexpected token in expression");
        }
    } else if (auto p = dynamic_cast<const number_literal *>(&node)) {
        program.code.emplace_back(sm::constant{p->num});
        return true;
    } else if (auto p = dynamic_cast<const binexpr *>(&node)) {
        if (p->op == SEMICOLON) {
            emit_discard(*p->lhs);
            return emit(*p->rhs);
        }
        if (p->op == ASSIGN) {
            auto target = dynamic_cast<const token_node *>(p->lhs.get());
            if (!target || target->tok.type != LIDENT)
                throw std::runtime_error("assignment target must be a variable");
            auto destination = environment.lookupName(target->tok.text);
            emit_value(*p->rhs);
            program.code.emplace_back(sm::store{destination});
            return true;
        }
        auto op = binary_operator(p->op);
        emit_value(*p->lhs);
        emit_value(*p->rhs);
        program.code.emplace_back(sm::binary{op});
        return true;
    } else if (auto p = dynamic_cast<const unary_neg *>(&node)) {
        program.code.emplace_back(sm::constant{0});
        emit_value(*p->operand);
        program.code.emplace_back(sm::binary{sm::binary_op::minus});
        return true;
    } else if (auto p = dynamic_cast<const builtin_call *>(&node)) {
        if (p->name.type == READ && !p->arg) {
            program.code.emplace_back(sm::read{});
            return true;
        }
        if (p->name.type == WRITE && p->arg) {
            emit_value(*p->arg);
            program.code.emplace_back(sm::write{});
            program.code.emplace_back(sm::drop{});
            return false;
        }
        throw std::runtime_error("invalid builtin call");
    } else if (auto p = dynamic_cast<const var_def_item_node *>(&node)) {
        auto destination = environment.lookupName(p->name.text);
        if (p->init)
            emit_value(*p->init);
        else /* assign to zero by default */
            program.code.emplace_back(sm::constant{0});
        program.code.emplace_back(sm::store{destination});
        program.code.emplace_back(sm::drop{});
        return false;
    } else if (auto p = dynamic_cast<const var_def_node *>(&node)) {
        emit_discard(*p->seq);
        return false;
    } else if (auto p = dynamic_cast<const scope_expr_node *>(&node)) {
        environment.scopes.emplace_back();
        if (p->defs) {
            /* declare and then assign */
            declare(*p->defs);
            emit_discard(*p->defs);
        }
        bool value = p->expr && emit(*p->expr);
        environment.scopes.pop_back();
        return value;
    } else if (auto p = dynamic_cast<const seq_node *>(&node)) {
        for (const auto &item : p->items)
            emit_discard(*item);
        return false;
    }
    throw std::runtime_error("unsupported AST node");
}

}

sm::program
compile(const ast_node &root)
{
    compiler ctx;
    ctx.emit_discard(root);
    return std::move(ctx.program);
}
