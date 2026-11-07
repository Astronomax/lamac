#include "x86.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <sstream>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <variant>
#include <vector>

namespace x86 {

enum class reg {
    ebx = 0,    /* Symbolic stack values (temporary slot) */
    ecx,        /* Symbolic stack values (temporary slot) */
    esi,        /* Symbolic stack values (temporary slot) */
    edi,        /* Symbolic stack values (temporary slot) */
    eax,        /* Temporal calculations, result of a call, division */
    edx,        /* The higher-order part of the dividend and the remainder */
    ebp,        /* Frame base of the current function */
    esp,        /* Hardware stack pointer */
    al
};

constexpr std::array reg_names = {
    "%ebx",
    "%ecx",
    "%esi",
    "%edi",
    "%eax",
    "%edx",
    "%ebp",
    "%esp",
    "%al"
};

constexpr std::array allocatable_regs = {
    reg::ebx,
    reg::ecx,
    reg::esi,
    reg::edi
};

/*
 * - A symbolic stack distributes values ​​among `allocatable_regs` and,
 * subsequently, memory slots.
 * - `eax` and `edx` are available to the generator as temporary registers,
 * with due regard for the implicit operands of instructions.
 * - Function calls preserve values ​​that must persist across the call.
 * - The prologue and epilogue adhere to the chosen ABI.
 * - Local variables and temporary slots are addressed relative to `ebp`.
 */

enum {
    WORD_SIZE = 4
};

/* We need to distinguish the following operand types: */
struct stack_slot {
    size_t index;
};

struct global_memory {
    std::string name;
};

struct immediate {
    int32_t value;
};

using operand = std::variant<
    reg,
    stack_slot,
    global_memory,
    immediate
    /* Indirect operands are not yet being used */
>;

struct compiler {
    /*
     * Temporary slots ​​of the computation stack. Some kind of simple register
     * allocation.
     *
     * Example:
     * LOAD x reads a variable from its assigned location and pushes a copy of
     * its value in some temporary slot. The code generator allocates a register
     * for this copy, or a temporary stack slot if no registers remain.
     * The variable's own slot and the temporary slot holding the loaded value
     * are separate locations.
     *
     * Imagine the program has two local variables, thus S(0) and S(1) are
     * reserved for them.
     *
     * With four registers and two local variables, locations are assigned by
     * symbolic stack depth as follows:
     *
     *   Stack position:  0    1    2    3    4     5
     *   Location:       ebx  ecx  esi  edi  S(2)  S(3)
     *
     * BINOP pops its two operands from the symbolic stack:
     *
     *   Before:     [ebx, ecx, esi, edi, S(2), S(3)]
     *   After pops: [ebx, ecx, esi, edi]
     *
     * The next allocate() sees depth 4 and reuses S(2) for the result.
     * S(3) is also available and can be reused when the stack grows again.
     *
     * Logical release and physical memory allocation are separate:
     *
     * - pop_back() removes a temporary slot from the symbolic stack, making
     *   its location available for reuse.
     * - Space for the maximum number of temporary slots is reserved once in
     *   the function prologue and released when the function returns.
     *   There is no need to adjust esp after each operation.
     *
     * Thus, stack_slots records peak usage. It determines the frame size
     * without preventing slot reuse.
     */
    std::vector<operand> symbolic_stack;
    size_t local_count = 0;
    size_t stack_slots = 0;

    std::unordered_map<std::string, std::vector<operand>> label_stacks;

    operand stack_location(size_t depth);
    std::vector<operand> make_stack(size_t depth);
    operand allocate();
    void materialize_stack(std::stringstream &code);
    void retrieve_stack(const std::string &label);

    std::vector<reg> live_registers(size_t depth);
};

/*
 * Returns the canonical location for the given symbolic stack depth.
 * [ebx, ecx, esi, edi, hardware stack -> ...]
 * Updates the required temporary slot count.
 */
operand
compiler::stack_location(size_t depth)
{
    if (depth < allocatable_regs.size())
        return allocatable_regs[depth];
    size_t index = depth - allocatable_regs.size();
    stack_slots = std::max(stack_slots, index + 1);
    return stack_slot{local_count + index};
}

std::vector<operand>
compiler::make_stack(size_t depth)
{
    std::vector<operand> stack;
    stack.reserve(depth);
    for (size_t i = 0; i < depth; ++i)
        stack.push_back(stack_location(i));
    return stack;
}

/* Allocates a new position on the symbolic stack and returns its location. */
operand
compiler::allocate()
{
    auto result = stack_location(symbolic_stack.size());
    symbolic_stack.push_back(result);
    return result;
}

/*
 * Collect live registers past given symbolic stack @depth (>= @depth).
 * The stack top has a depth 0.
 */
std::vector<reg>
compiler::live_registers(size_t depth)
{
    assert(depth <= symbolic_stack.size());
    std::vector<reg> regs;
    for (size_t i = 0; i < symbolic_stack.size() - depth; i++)
        if (auto *r = std::get_if<reg>(&symbolic_stack[i]))
            regs.push_back(*r);
    return regs;
}

operand
sm_location_to_operand(const sm::location &location)
{
    switch (location.kind) {
        case (sm::location_kind::global):
        return global_memory{"global_" + std::get<std::string>(location.value)};
        case (sm::location_kind::local):
        return stack_slot{std::get<uint32_t>(location.value)};
        case (sm::location_kind::argument):
        throw std::runtime_error("unimplemented"); 
        case (sm::location_kind::captured):
        throw std::runtime_error("unimplemented");
        default:
        abort();
    }
}

std::string
operand_to_string(const operand &operand)
{
    if (const auto *reg_op = std::get_if<reg>(&operand)) {
        return reg_names[static_cast<size_t>(*reg_op)];
    } else if (const auto *stack_slot_op = std::get_if<stack_slot>(&operand)) {
        auto index = stack_slot_op->index;
        auto offset = (index + 1) * WORD_SIZE;
        return "-" + std::to_string(offset) + "(%ebp)";
    } else if (const auto *global_op = std::get_if<global_memory>(&operand)) {
        return global_op->name;
    } else if (const auto *immediate_op = std::get_if<immediate>(&operand)) {
        return "$" + std::to_string(immediate_op->value);
    } else {
        abort();
    }
}

using instruction_arg = std::variant<operand, std::string>;

void
code_append_instruction(std::stringstream &code, const std::string &insn,
                        std::initializer_list<instruction_arg> args = {})
{
    code << '\t' << insn;
    const char *separator = "\t";
    for (const auto &arg : args) {
        code << separator;
        if (const auto *op = std::get_if<operand>(&arg))
            code << operand_to_string(*op);
        else
            code << std::get<std::string>(arg);
        separator = ",\t";
    }
    code << '\n';
}

bool
is_cmp(const sm::binary_op &op)
{
    return  op == sm::binary_op::le     ||
            op == sm::binary_op::leq    ||
            op == sm::binary_op::ge     ||
            op == sm::binary_op::geq    ||
            op == sm::binary_op::eq     ||
            op == sm::binary_op::neq;
}

bool
operand_resides_in_memory(const operand &operand)
{
  return std::holds_alternative<global_memory>(operand) ||
         std::holds_alternative<stack_slot>(operand);
}

void
code_append_move(std::stringstream &code, const operand &src, const operand &dst)
{
    if (operand_resides_in_memory(src) && 
        operand_resides_in_memory(dst)) {
            /* Move through eax register */
            code_append_instruction(code, "movl", {src, reg::eax});
            code_append_instruction(code, "movl", {reg::eax, dst});
            return;
    }
    /* Move directly */
    code_append_instruction(code, "movl", {src, dst});
}

void
compiler::materialize_stack(std::stringstream &code)
{
    for (size_t depth = 0; depth < symbolic_stack.size(); ++depth) {
        auto destination = stack_location(depth);
        auto &source = symbolic_stack[depth];
        /*
         * Allocated values already occupy their depth's canonical location.
         * Only immediates need materializing.
         */
        if (std::holds_alternative<immediate>(source)) {
            code_append_move(code, source, destination);
        } else if (const auto *r = std::get_if<reg>(&source)) {
            assert(std::holds_alternative<reg>(destination) &&
                   *r == std::get<reg>(destination));
        } else if (const auto *slot = std::get_if<stack_slot>(&source)) {
            /*
             * Load always duplicates variable value onto the stack. We can't do
             * otherwise because the variable's value may change later. We need
             * to copy its value onto the stack at this specific point in time.
             */
            assert(std::holds_alternative<stack_slot>(destination) &&
                   slot->index == std::get<stack_slot>(destination).index);
        } else {
            /*
             * We don't push globals onto the symbolic stack (see the 'load'
             * code generator). Unreachable.
             */
            abort();
        }
        source = destination;
    }
}

void
compiler::retrieve_stack(const std::string &label)
{
    symbolic_stack = label_stacks.at(label);
}

/*
 * At control-flow joins, incoming values must occupy the same canonical
 * registers and stack slots regardless of the path taken. Code generation
 * therefore materializes the symbolic stack on incoming paths and restores
 * its canonical layout at each label. This layout is determined by stack
 * depth, which must be known for every reachable label before emission,
 * including labels first reached by a backward jump.
 *
 * This separate pass traverses the control-flow graph using dfs to determine
 * the entry depth of each reachable instruction. It ensures that all incoming
 * paths agree on that depth and that the stack contains enough operands to
 * execute the instruction. Unreachable instructions have no entry depth.
 */
std::vector<std::optional<size_t>>
calculate_entry_depths(const sm::program &program)
{
    std::unordered_map<std::string, size_t> labels;
    for (size_t i = 0; i < program.code.size(); ++i)
        if (auto p = std::get_if<sm::label>(&program.code[i]))
            if (!labels.emplace(p->label, i).second)
                throw std::runtime_error("duplicate label: " + p->label);
    auto get_label_pc = [&](const std::string &label) {
        auto it = labels.find(label);
        if (it == labels.end())
            throw std::runtime_error("undefined label: " + label);
        return it->second;
    };
    std::vector<std::optional<size_t>> depths(program.code.size() + 1);
    std::vector<size_t> pending;
    auto visit = [&](size_t pc, size_t depth) {
        if (depths[pc]) {
            /* Ensure that all paths enter at the same depth. */
            if (*depths[pc] != depth)
                throw std::runtime_error("inconsistent stack depth at instruction "
                                         + std::to_string(pc));
        } else {
            depths[pc] = depth;
            pending.push_back(pc);
        }
    };
    visit(0, 0);
    while (!pending.empty()) {
        auto pc = pending.back();
        pending.pop_back();
        if (pc == program.code.size())
            continue;
        auto depth = *depths[pc];
        const auto &insn = program.code[pc];
        size_t required = 0;
        if (std::holds_alternative<sm::binary>(insn))
            required = 2;
        else if (std::holds_alternative<sm::drop>(insn) ||
                 std::holds_alternative<sm::cjmp>(insn) ||
                 std::holds_alternative<sm::store>(insn) ||
                 std::holds_alternative<sm::write>(insn))
            required = 1;
        if (depth < required)
            throw std::runtime_error("symbolic stack underflow at instruction "
                                     + std::to_string(pc));
        if (std::holds_alternative<sm::constant>(insn) ||
            std::holds_alternative<sm::load>(insn) ||
            std::holds_alternative<sm::read>(insn))
            ++depth;
        else if (std::holds_alternative<sm::binary>(insn) ||
                 std::holds_alternative<sm::drop>(insn) ||
                 std::holds_alternative<sm::cjmp>(insn))
            --depth;
        if (auto p = std::get_if<sm::jmp>(&insn)) {
            visit(get_label_pc(p->label), depth);
        } else {
            if (auto p = std::get_if<sm::cjmp>(&insn))
                visit(get_label_pc(p->label), depth);
            visit(pc + 1, depth);
        }
    }
    return depths;
}

void
code_append_function_call(std::stringstream &code, compiler &ctx,
                          std::string label, int n_args)
{
    /*
     * Collect the live registers in order to save their values ​​and
     * free them up for the duration of the callee execution.
     *
     * Skip the arguments, as they are only needed for the purpose of
     * placing them onto the hardware stack (which we will do next).
     */
    const auto &live_regs = ctx.live_registers(n_args);
    /* Save live registers from the symbolic stack to the hardware one. */
    for (const auto &reg : live_regs)
        code_append_instruction(code, "pushl", {reg});
    /*
     * Push arguments onto the hardware stack (some may already be there, while
     * others may be immediate values or reside in registers or global memory).
     */
    assert(ctx.symbolic_stack.size() >= n_args);
    auto begin = ctx.symbolic_stack.end() - n_args;
    auto end = ctx.symbolic_stack.end();
    for (auto arg = begin; arg != end; arg++)
        code_append_instruction(code, "pushl", {*arg});
    /*
     * Drop them from the symbolic stack since they were only needed to prepare
     * the arguments on the hardware stack.
     */
    ctx.symbolic_stack.erase(begin, end);
    /* Call function. */
    code_append_instruction(code, "call", {label});
    /* Drop the arguments (adjust hardware stack pointer back). */
    code_append_instruction(code, "addl", {immediate{WORD_SIZE * n_args}, reg::esp});
    /* Restore saved live registers. */
    for (auto reg = live_regs.rbegin(); reg != live_regs.rend(); ++reg)
        code_append_instruction(code, "pop", {*reg});
}

std::string
compile(const sm::program &p_sm)
{
    compiler ctx;
    ctx.local_count = p_sm.local_count;
    std::stringstream code;
    auto depths = calculate_entry_depths(p_sm);
    for (size_t i = 0; i < p_sm.code.size(); ++i) {
        if (!depths[i])
            /* Unreachable code. */
            continue;
        if (auto p = std::get_if<sm::label>(&p_sm.code[i])) {
            ctx.label_stacks.emplace(p->label, ctx.make_stack(*depths[i]));
        }
    }
    bool falls_through = true;

    code << "\t.global\tmain\n";
    /* The runtime scans this section for global GC roots. */
    code << "\t.section\tcustom_data,\"aw\",@progbits\n";
    code << "\t.balign\t" << WORD_SIZE << '\n';
    /* Global variables will not be visible outside this file. */
    for (const auto &insn : p_sm.code)
        if (const auto *global = std::get_if<sm::global>(&insn))
            code << "global_" << global->name << ":\t.int\t0\n";
    code << "\t.text\n";
    code << "main:\n";
    code_append_instruction(code, "pushl", {reg::ebp});
    code_append_move(code, reg::esp, reg::ebp);
    code_append_instruction(code, "subl", {"$main_SIZE", reg::esp});

    for (size_t pc = 0; pc < p_sm.code.size(); ++pc) {
        if (!depths[pc])
            /* Unreachable code. */
            continue;
        const auto &insn = p_sm.code[pc];
        if (std::holds_alternative<sm::global>(insn)) {
            continue;
        } else if (std::holds_alternative<sm::read>(insn)) {
            code_append_function_call(code, ctx, "Lread", 0);
            /*
             * The i386 System V ABI returns an int in eax, so Lread leaves its
             * tagged result there. An arithmetic right shift removes the tag
             * while preserving the sign.
             */
            code_append_instruction(code, "sarl", {immediate{1}, reg::eax});
            code_append_move(code, reg::eax, ctx.allocate());
        } else if (std::holds_alternative<sm::write>(insn)) {
            /* WRITE keeps its value. The call consumes only the argument copy. */
            assert(!ctx.symbolic_stack.empty());
            const auto argument = ctx.symbolic_stack.back();
            /* BOX(n) = (n << 1) | 1 */
            code_append_move(code, argument, reg::eax);
            code_append_instruction(code, "shll", {immediate{1}, reg::eax});
            code_append_instruction(code, "orl", {immediate{1}, reg::eax});
            ctx.symbolic_stack.push_back(reg::eax);
            code_append_function_call(code, ctx, "Lwrite", 1);
        } else if (std::holds_alternative<sm::drop>(insn)) {
            /* Drop only affects a symbolic stack */
            assert(!ctx.symbolic_stack.empty());
            ctx.symbolic_stack.pop_back();
        } else if (std::holds_alternative<sm::binary>(insn)) {
            const auto &binary_insn = std::get<sm::binary>(insn);
            const auto rhs = ctx.symbolic_stack.back();
            ctx.symbolic_stack.pop_back();
            const auto lhs = ctx.symbolic_stack.back();
            ctx.symbolic_stack.pop_back();
            const auto result = ctx.allocate();
            if (is_cmp(binary_insn.op)) {
                /* One of the operands must be a register, so move lhs to eax */
                code_append_move(code, lhs, reg::eax);
                code_append_instruction(code, "cmpl", {rhs, reg::eax});
                /*
                 * SETL r/m8
                 * In 32-bit x86 mode, not all registers allow access to their
                 * 8-bit parts. Therefore, use eax (al) as an intermediate storage.
                 *
                 * Clear the entire eax before setting result into a lower 8 bits.
                 */
                code_append_move(code, immediate{0}, reg::eax);
                /* Set result into a lower 8 bits (1 or 0). */
                switch (binary_insn.op) {
                    case (sm::binary_op::le):
                    code_append_instruction(code, "setl", {reg::al});
                    break;
                    case (sm::binary_op::leq):
                    code_append_instruction(code, "setle", {reg::al});
                    break;
                    case (sm::binary_op::ge):
                    code_append_instruction(code, "setg", {reg::al});
                    break;
                    case (sm::binary_op::geq):
                    code_append_instruction(code, "setge", {reg::al});
                    break;
                    case (sm::binary_op::eq):
                    code_append_instruction(code, "sete", {reg::al});
                    break;
                    case (sm::binary_op::neq):
                    code_append_instruction(code, "setne", {reg::al});
                    break;
                    default:
                    abort();
                }
                code_append_move(code, reg::eax, result);
            } else if (binary_insn.op == sm::binary_op::plus ||
                       binary_insn.op == sm::binary_op::minus ||
                       binary_insn.op == sm::binary_op::mul) {
                /* One of the operands must be a register, so move lhs to eax */
                code_append_move(code, lhs, reg::eax);
                switch (binary_insn.op) {
                    case (sm::binary_op::plus):
                    code_append_instruction(code, "addl", {rhs, reg::eax});
                    break;
                    case (sm::binary_op::minus):
                    code_append_instruction(code, "subl", {rhs, reg::eax});
                    break;
                    case (sm::binary_op::mul):
                    code_append_instruction(code, "imull", {rhs, reg::eax});
                    break;
                    default:
                    abort();
                }
                code_append_move(code, reg::eax, result);
            } else if (binary_insn.op == sm::binary_op::conjunction ||
                       binary_insn.op == sm::binary_op::disjunction) {
                /*
                 * Normalize operands to 0 or 1 to match the original regression
                 * tests, which use logical results in arithmetic and comparisons.
                 * The specification's zero/non-zero convention allows preserving
                 * the upper bits here, but those tests require canonical results.
                 * "any non-zero value is treated as truth and zero as falsity,
                 * AND THE RESULT RESPECTS THIS CONVENTION".
                 * MOV clears eax without changing the flags used by SETNE.
                 */
                code_append_move(code, rhs, reg::eax);
                code_append_instruction(code, "cmpl", {immediate{0}, reg::eax});
                code_append_move(code, immediate{0}, reg::eax);
                code_append_instruction(code, "setne", {reg::al});
                code_append_move(code, reg::eax, reg::edx);
                /* Same for the lhs. */
                code_append_move(code, lhs, reg::eax);
                code_append_instruction(code, "cmpl", {immediate{0}, reg::eax});
                code_append_move(code, immediate{0}, reg::eax);
                code_append_instruction(code, "setne", {reg::al});
                switch (binary_insn.op) {
                    case (sm::binary_op::conjunction):
                    code_append_instruction(code, "andl", {reg::edx, reg::eax});
                    break;
                    case (sm::binary_op::disjunction):
                    code_append_instruction(code, "orl", {reg::edx, reg::eax});
                    break;
                    default:
                    abort();
                }
                code_append_move(code, reg::eax, result);
            } else if (binary_insn.op == sm::binary_op::div) {
                /*
                 * "Divides the (signed) value in the AX, DX:AX, or EDX:EAX (dividend) by
                 * the source operand (divisor) and stores the result in the AX (AH:AL),
                 * DX:AX, or EDX:EAX registers". So move lhs (dividend) to eax;
                */
                code_append_move(code, lhs, reg::eax);
                /* idivl divides EDX:EAX, so let's extend 32-bit eax to 64-bit edx:eax. */
                code_append_instruction(code, "cltd", {});
                auto divisor = rhs;
                if (std::holds_alternative<immediate>(divisor)) {
                    /* IDIV accepts only a register or memory operand. */
                    code_append_move(code, divisor, result);
                    divisor = result;
                }
                code_append_instruction(code, "idivl", {divisor});
                code_append_move(code, reg::eax, result);
            } else if (binary_insn.op == sm::binary_op::mod) {
                /*
                 * idivl consumes a signed 64-bit dividend in edx:eax, but produces
                 * a signed 32-bit quotient in eax and a 32-bit remainder in edx.
                 * The original high half in edx is consumed, then overwritten.
                 * If the quotient does not fit in 32 bits, idivl raises #DE
                 * rather than returning a 64-bit quotient.
                 */
                code_append_move(code, lhs, reg::eax);
                code_append_instruction(code, "cltd", {});
                auto divisor = rhs;
                if (std::holds_alternative<immediate>(divisor)) {
                    /* Reuse the result location to hold an immediate divisor. */
                    code_append_move(code, divisor, result);
                    divisor = result;
                }
                code_append_instruction(code, "idivl", {divisor});
                code_append_move(code, reg::edx, result);
            } else {
                abort();
            }
        } else if (std::holds_alternative<sm::load>(insn)) {
            const auto &load_insn = std::get<sm::load>(insn);
            const auto destination = ctx.allocate();
            code_append_move(code, sm_location_to_operand(load_insn.source), destination);
        } else if (std::holds_alternative<sm::store>(insn)) {
            const auto &store_insn = std::get<sm::store>(insn);
            /* STORE leaves the assigned value on the symbolic stack. */
            assert(!ctx.symbolic_stack.empty());
            const auto source = ctx.symbolic_stack.back();
            code_append_move(code, source, sm_location_to_operand(store_insn.destination));
        } else if (std::holds_alternative<sm::constant>(insn)) {
            const auto &constant_insn = std::get<sm::constant>(insn);
            ctx.symbolic_stack.emplace_back(immediate{constant_insn.value});
        } else if (std::holds_alternative<sm::label>(insn)) {
            const auto &label = std::get<sm::label>(insn).label;
            if (falls_through) {
                ctx.materialize_stack(code);
                assert(ctx.symbolic_stack.size() == ctx.label_stacks.at(label).size());
            } else {
                ctx.retrieve_stack(label);
            }
            code << label << ":\n";
            falls_through = true;
        } else if (std::holds_alternative<sm::jmp>(insn)) {
            const auto &label = std::get<sm::jmp>(insn).label;
            ctx.materialize_stack(code);
            code_append_instruction(code, "jmp", {label});
            falls_through = false;
        } else if (std::holds_alternative<sm::cjmp>(insn)) {
            const auto &jump = std::get<sm::cjmp>(insn);
            const auto condition = ctx.symbolic_stack.back();
            ctx.symbolic_stack.pop_back();
            /* Preserve the condition while materializing the remaining values. */
            code_append_move(code, condition, reg::edx);
            ctx.materialize_stack(code);
            code_append_instruction(code, "cmpl", {immediate{0}, reg::edx});
            switch (jump.condition) {
                case sm::jump_condition::zero:
                code_append_instruction(code, "je", {jump.label});
                break;
                case sm::jump_condition::not_zero:
                code_append_instruction(code, "jne", {jump.label});
                break;
                default:
                throw std::runtime_error("unsupported jump condition");
            }
        } else {
            abort();
        }
    }
    code_append_move(code, reg::ebp, reg::esp);
    code_append_instruction(code, "popl", {reg::ebp});
    code_append_move(code, immediate{0}, reg::eax);
    code_append_instruction(code, "ret");
    code << "\t.set\tmain_SIZE,\t"
         << (ctx.local_count + ctx.stack_slots) * WORD_SIZE << '\n';
    return code.str();
}

} /* namespace x86 */
