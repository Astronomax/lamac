#include "sm.h"

#include <cassert>
#include <optional>
#include <queue>
#include <stdexcept>
#include <unordered_map>

namespace sm {

struct function_binding {
    std::string label;
    size_t argument_count;
};

using name_binding = std::variant<
    sm::location,
    function_binding
>;

struct scope {
    std::unordered_map<std::string, name_binding> names;
};

struct env {
    std::vector<scope> scopes;
    int n_labels = 0;

    void
    add_name(std::string name, name_binding binding);

    std::optional<name_binding>
    lookup_name(const std::string &name) const;

    sm::location
    lookup_variable(const std::string &name) const;

    function_binding
    lookup_function(const std::string &function_name) const;

    std::string
    gen_label();
};

void
env::add_name(std::string name, name_binding binding)
{
    assert(!scopes.empty());
    auto [it, inserted] = scopes.back().names.emplace(
        std::move(name), std::move(binding));
    if (!inserted)
        throw std::runtime_error("duplicate \"" + name + "\" name definition");
}

std::optional<name_binding>
env::lookup_name(const std::string &name) const
{
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->names.find(name);
        if (found != it->names.end())
            return found->second;
    }
    return std::nullopt;
}

sm::location
env::lookup_variable(const std::string &variable_name) const
{
    if (const auto name = lookup_name(variable_name); name)
        if (const auto *location = std::get_if<sm::location>(&name.value()))
            return *location;
    throw std::runtime_error("the name \"" + variable_name +
                             "\" does not designate a variable");
}

function_binding
env::lookup_function(const std::string &function_name) const
{
    if (const auto name = lookup_name(function_name); name)
        if (const auto *binding = std::get_if<function_binding>(&name.value()))
            return *binding;
    throw std::runtime_error("the name \"" + function_name +
                             "\" does not designate a function");
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

struct function_compile_task {
    const fun_definition_node *definition;
    std::string label;
    env environment;
};

struct compiler {
    sm::program program;
    // TODO: Reuse slots on scope exit, track peak locals per function.
    uint32_t local_count = 0;
    env environment;
    std::vector<function_compile_task> pending_functions;

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

    void
    remember_functions(const ast_node &node);

    void
    compile_functions();
};

void
compiler::declare(const ast_node &node)
{
    if (auto p = dynamic_cast<const var_def_item_node *>(&node)) {
        bool global = environment.scopes.size() == 1;
        if (global) {
            environment.add_name(p->name.text,
                sm::location{sm::location_kind::global, p->name.text});
            ++program.global_count;
            program.code.emplace_back(sm::global{p->name.text});
        } else {
            environment.add_name(p->name.text,
                sm::location{sm::location_kind::local, local_count});
            ++local_count;
        }
    } else if (auto p = dynamic_cast<const var_def_node *>(&node)) {
        /* ignore is_public for now */
        declare(*p->seq);
    } else if (auto p = dynamic_cast<const fun_definition_node *>(&node)) {
        /*
         * TODO: Introduce gen_function_label to generate readable, predictable
         * function labels.
         */
        auto function_label = environment.gen_label();
        environment.add_name(p->name.text,
                             function_binding{std::move(function_label), p->args.size()});
    } else if (auto p = dynamic_cast<const seq_node *>(&node)) {
        for (const auto &item : p->items)
            declare(*item);
    } else {
        throw std::runtime_error("variable definition expected");
    }
}

void
compiler::remember_functions(const ast_node &node)
{
    if (auto p = dynamic_cast<const fun_definition_node *>(&node)) {
        auto binding = environment.lookup_function(p->name.text);
        pending_functions.push_back({p, std::move(binding.label), environment});
    } else if (auto p = dynamic_cast<const seq_node *>(&node)) {
        for (const auto &item : p->items)
            remember_functions(*item);
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
                program.code.emplace_back(sm::load{environment.lookup_variable(p->tok.text)});
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
            auto destination = environment.lookup_variable(target->tok.text);
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
        auto destination = environment.lookup_variable(p->name.text);
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
    } else if (dynamic_cast<const fun_definition_node *>(&node)) {
        /* The body is compiled later from pending_functions. */
        finish(false);
        return false;
    } else if (auto p = dynamic_cast<const scope_expr_node *>(&node)) {
        environment.scopes.emplace_back();
        if (p->defs) {
            /* declare and then assign */
            declare(*p->defs);
            /*
             * All functions have been added to the state, so we can now create
             * compilation tasks, each with a copy of this state.
             */
            remember_functions(*p->defs);

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
    } else if (auto p = dynamic_cast<const fun_call_node *>(&node)) {
        /*
         * Only direct calls to named functions are supported for now.
         * Functions are not first-class values, so the callee must be an
         * identifier resolved to a function binding at compile time.
         */
        auto name_token = dynamic_cast<const token_node *>(p->callee.get());
        if (!name_token || name_token->tok.type != LIDENT)
            throw std::runtime_error("only direct calls to named functions are supported");
        auto function_name = name_token->tok.text;
        auto binding = environment.lookup_function(function_name);
        if (binding.argument_count != p->args.size())
            throw std::runtime_error("function \"" + function_name + "\" expects " +
                                     std::to_string(binding.argument_count) +
                                     " arguments, got " + std::to_string(p->args.size()));
        for (const auto &arg : p->args) {
            auto arg_exit_label = environment.gen_label();
            bool arg_exit_used = emit(*arg, value_context::required, arg_exit_label);
            emit_label(arg_exit_label, arg_exit_used);
        }
        program.code.emplace_back(sm::call{binding.label, binding.argument_count});
        finish(true);
        return false;
    }
    throw std::runtime_error("unsupported AST node");
}

void
compiler::compile_functions()
{
    auto scopes = std::move(environment.scopes);
    auto saved_local_count = local_count;
    while (!pending_functions.empty()) {
        std::vector<function_compile_task> functions;
        functions.swap(pending_functions);
        for (auto &function : functions) {
            environment.scopes = std::move(function.environment.scopes);
            /*
             * Captures are not supported: retain only the global scope.
             * Keep the current label counter to avoid reusing labels.
             */
            assert(!environment.scopes.empty());
            environment.scopes.resize(1);
            local_count = 0;
            /* Create a new scope with arguments. */
            environment.scopes.emplace_back();
            uint32_t n_args = 0;
            for (const auto &arg_name : function.definition->args)
                environment.add_name(arg_name,
                    sm::location{sm::location_kind::argument, n_args++});
            /* Compile the body before filling in its local slot count. */
            program.code.emplace_back(sm::label{function.label});
            const size_t begin_index = program.code.size();
            program.code.emplace_back(sm::begin{function.label, n_args, 0});
            auto function_end_label = environment.gen_label();
            bool function_end_used = emit(*function.definition->body,
                                         value_context::required, function_end_label);
            std::get<sm::begin>(program.code[begin_index]).n_locals = local_count;
            emit_label(function_end_label, function_end_used);
            program.code.emplace_back(sm::end{});
        }
    }
    environment.scopes = std::move(scopes);
    local_count = saved_local_count;
}

sm::program
compile(const ast_node &root)
{
    compiler ctx;
    ctx.program.code.emplace_back(sm::label{"main"});
    const size_t begin_index = ctx.program.code.size();
    ctx.program.code.emplace_back(sm::begin{"main", 0, 0});
    auto exit_label = ctx.environment.gen_label();
    bool exit_used = ctx.emit(root, value_context::discarded, exit_label);
    ctx.emit_label(exit_label, exit_used);
    std::get<sm::begin>(ctx.program.code[begin_index]).n_locals = ctx.local_count;
    ctx.program.code.emplace_back(sm::end{});
    ctx.compile_functions();
    return std::move(ctx.program);
}

} /* namespace sm */
