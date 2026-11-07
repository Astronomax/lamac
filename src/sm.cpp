#include "sm.h"

#include <stdexcept>
#include <unordered_map>

namespace sm {

struct scope {
    std::unordered_map<std::string, sm::location> vars;
};

struct env {
    std::vector<scope> scopes;
    int n_labels = 0;

    sm::location
    lookup_name(const std::string &name) const;

    std::string
    gen_label();
};

sm::location
env::lookup_name(const std::string &name) const
{
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->vars.find(name);
        if (found != it->vars.end())
            return found->second;
    }
    throw std::runtime_error("undefined variable: " + name);
}

std::string
env::gen_label() {
    return "L" + std::to_string(n_labels++);
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

enum class value_context {
    required,
    discarded,
};

struct compiler {
    sm::program program;
    env environment;

    bool
    emit(const ast_node &node, value_context context, const std::string &exit_label);

    inline void
    emit_label(const std::string &label, bool used)
    {
        if (used)
            program.code.emplace_back(sm::label{label});
    }

    void
    declare(const ast_node &node);
};

void
compiler::declare(const ast_node &node)
{
    if (auto p = dynamic_cast<const var_def_item_node *>(&node)) {
        bool global = environment.scopes.size() == 1;
        auto &count = global ? program.global_count : program.local_count;
        auto kind = global ? sm::location_kind::global : sm::location_kind::local;
        auto &vars = environment.scopes.back().vars;
        sm::location location{kind, count};
        if (global)
            location.value = p->name.text;
        if (!vars.emplace(p->name.text, std::move(location)).second)
            throw std::runtime_error("duplicate variable: " + p->name.text);
        ++count;
        if (global)
            program.code.emplace_back(sm::global{p->name.text});
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
compiler::emit(const ast_node &node, value_context context, const std::string &exit_label)
{
    auto finish = [&](bool has_value) -> void {
        if (context == value_context::required && !has_value)
            throw std::runtime_error("value expected");
        if (context == value_context::discarded && has_value)
            program.code.emplace_back(sm::drop{});
    };
    if (auto p = dynamic_cast<const token_node *>(&node)) {
        switch (p->tok.type) {
            case LIDENT:
                program.code.emplace_back(sm::load{environment.lookup_name(p->tok.text)});
                finish(true);
                return false;
            case TRUE:
                program.code.emplace_back(sm::constant{1});
                finish(true);
                return false;
            case FALSE:
                program.code.emplace_back(sm::constant{0});
                finish(true);
                return false;
            case SKIP:
                finish(false);
                return false;
            default:
                throw std::runtime_error("unexpected token in expression");
        }
    } else if (auto p = dynamic_cast<const number_literal *>(&node)) {
        program.code.emplace_back(sm::constant{p->num});
        finish(true);
        return false;
    } else if (auto p = dynamic_cast<const binexpr *>(&node)) {
        if (p->op == SEMICOLON) {
            auto lhs_exit_label = environment.gen_label();
            bool lhs_exit_used = emit(*p->lhs, value_context::discarded, lhs_exit_label);
            emit_label(lhs_exit_label, lhs_exit_used);
            return emit(*p->rhs, context, exit_label);
        }
        if (p->op == ASSIGN) {
            auto target = dynamic_cast<const token_node *>(p->lhs.get());
            if (!target || target->tok.type != LIDENT)
                throw std::runtime_error("assignment target must be a variable");
            auto destination = environment.lookup_name(target->tok.text);
            auto rhs_exit_label = environment.gen_label();
            bool rhs_exit_used = emit(*p->rhs, value_context::required, rhs_exit_label);
            emit_label(rhs_exit_label, rhs_exit_used);
            program.code.emplace_back(sm::store{destination});
            finish(true);
            return false;
        }
        auto op = binary_operator(p->op);
        auto lhs_exit_label = environment.gen_label();
        bool lhs_exit_used = emit(*p->lhs, value_context::required, lhs_exit_label);
        emit_label(lhs_exit_label, lhs_exit_used);
        auto rhs_exit_label = environment.gen_label();
        bool rhs_exit_used = emit(*p->rhs, value_context::required, rhs_exit_label);
        emit_label(rhs_exit_label, rhs_exit_used);
        program.code.emplace_back(sm::binary{op});
        finish(true);
        return false;
    } else if (auto p = dynamic_cast<const unary_neg *>(&node)) {
        program.code.emplace_back(sm::constant{0});
        auto operand_exit_label = environment.gen_label();
        bool operand_exit_used = emit(*p->operand, value_context::required, operand_exit_label);
        emit_label(operand_exit_label, operand_exit_used);
        program.code.emplace_back(sm::binary{sm::binary_op::minus});
        finish(true);
        return false;
    } else if (auto p = dynamic_cast<const builtin_call *>(&node)) {
        if (p->name.type == READ && !p->arg) {
            program.code.emplace_back(sm::read{});
            finish(true);
            return false;
        }
        if (p->name.type == WRITE && p->arg) {
            auto arg_exit_label = environment.gen_label();
            bool arg_exit_used = emit(*p->arg, value_context::required, arg_exit_label);
            emit_label(arg_exit_label, arg_exit_used);
            program.code.emplace_back(sm::write{});
            program.code.emplace_back(sm::drop{});
            finish(false);
            return false;
        }
        throw std::runtime_error("invalid builtin call");
    } else if (auto p = dynamic_cast<const var_def_item_node *>(&node)) {
        auto destination = environment.lookup_name(p->name.text);
        if (p->init) {
            auto init_exit_label = environment.gen_label();
            bool init_exit_used = emit(*p->init, value_context::required, init_exit_label);
            emit_label(init_exit_label, init_exit_used);
        } else /* assign to zero by default */
            program.code.emplace_back(sm::constant{0});
        program.code.emplace_back(sm::store{destination});
        program.code.emplace_back(sm::drop{});
        finish(false);
        return false;
    } else if (auto p = dynamic_cast<const var_def_node *>(&node)) {
        auto seq_exit_label = environment.gen_label();
        bool seq_exit_used = emit(*p->seq, value_context::discarded, seq_exit_label);
        emit_label(seq_exit_label, seq_exit_used);
        finish(false);
        return false;
    } else if (auto p = dynamic_cast<const scope_expr_node *>(&node)) {
        environment.scopes.emplace_back();
        if (p->defs) {
            /* declare and then assign */
            declare(*p->defs);
            auto defs_exit_label = environment.gen_label();
            bool defs_exit_used = emit(*p->defs, value_context::discarded, defs_exit_label);
            emit_label(defs_exit_label, defs_exit_used);
        }
        bool exit_used = false;
        if (p->expr)
            exit_used = emit(*p->expr, context, exit_label);
        else
            finish(false);
        environment.scopes.pop_back();
        return exit_used;
    } else if (auto p = dynamic_cast<const seq_node *>(&node)) {
        for (const auto &item : p->items) {
            auto item_exit_label = environment.gen_label();
            bool item_exit_used = emit(*item, value_context::discarded, item_exit_label);
            emit_label(item_exit_label, item_exit_used);
        }
        finish(false);
        return false;
    } else if (auto p = dynamic_cast<const if_expr_node *>(&node)) {
        auto else_label = environment.gen_label();
        auto condition_exit_label = environment.gen_label();
        bool condition_exit_used = emit(*p->condition, value_context::required, condition_exit_label);
        emit_label(condition_exit_label, condition_exit_used);
        program.code.emplace_back(sm::cjmp{sm::jump_condition::zero, else_label});
        emit(*p->then_branch, context, exit_label);
        program.code.emplace_back(sm::jmp{exit_label});
        program.code.emplace_back(sm::label{else_label});
        if (p->else_branch)
            emit(*p->else_branch, context, exit_label);
        else
            finish(false);
        return true;
    } else if (auto p = dynamic_cast<const while_do_expr_node *>(&node)) {
        auto condition_label = environment.gen_label();
        auto condition_exit_label = environment.gen_label();
        auto body_label = environment.gen_label();
        program.code.emplace_back(sm::jmp{condition_label});
        program.code.emplace_back(sm::label{body_label});
        emit(*p->body, value_context::discarded, condition_label);
        program.code.emplace_back(sm::label{condition_label});
        bool condition_exit_used = emit(*p->condition, value_context::required, condition_exit_label);
        emit_label(condition_exit_label, condition_exit_used);
        program.code.emplace_back(sm::cjmp{sm::jump_condition::not_zero, body_label});
        finish(false);
        return false;
    } else if (auto p = dynamic_cast<const do_while_expr_node *>(&node)) {
        auto condition_label = environment.gen_label();
        auto condition_exit_label = environment.gen_label();
        auto body_label = environment.gen_label();
        program.code.emplace_back(sm::label{body_label});
        bool condition_used = emit(*p->body, value_context::discarded, condition_label);
        emit_label(condition_label, condition_used);
        bool condition_exit_used = emit(*p->condition, value_context::required, condition_exit_label);
        emit_label(condition_exit_label, condition_exit_used);
        program.code.emplace_back(sm::cjmp{sm::jump_condition::not_zero, body_label});
        finish(false);
        return false;

    }
    throw std::runtime_error("unsupported AST node");
}

sm::program
compile(const ast_node &root)
{
    compiler ctx;
    auto exit_label = ctx.environment.gen_label();
    bool exit_used = ctx.emit(root, value_context::discarded, exit_label);
    ctx.emit_label(exit_label, exit_used);
    return std::move(ctx.program);
}

} /* namespace sm */
