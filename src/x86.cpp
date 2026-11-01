#include "x86.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <sstream>
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

    operand allocate();

    std::vector<reg> live_registers(size_t depth);
};

/*
 * Allocates a new position on the symbolic stack.
 * [ebx, ecx, esi, edi, hardware stack -> ...]
 */
operand
compiler::allocate() {
    size_t depth = symbolic_stack.size();
    operand result;
    if (depth < allocatable_regs.size()) {
        result = allocatable_regs[depth];
    } else {
        size_t index = depth - allocatable_regs.size();
        result = stack_slot{local_count + index};
        stack_slots = std::max(stack_slots, index + 1);
    }
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
    }
    /* Move directly */
    code_append_instruction(code, "movl", {src, dst});
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

    for (const auto &insn : p_sm.code) {
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
                 * Set the least significant bit to 1 for non-zero numbers so that
                 * bitwise operations behave like logical ones.
                 */
                code_append_move(code, rhs, reg::eax);
                code_append_instruction(code, "cmp", {immediate{0}, reg::eax});
                code_append_instruction(code, "setne", {reg::al});
                code_append_move(code, reg::eax, rhs);
                /* Same for the lhs. */
                code_append_move(code, lhs, reg::eax);
                code_append_instruction(code, "cmp", {immediate{0}, reg::eax});
                code_append_instruction(code, "setne", {reg::al});
                /*
                 * One of the operands must be a register, so use eax without
                 * moving back to lhs.
                 */
                switch (binary_insn.op) {
                    case (sm::binary_op::conjunction):
                    code_append_instruction(code, "andl", {rhs, reg::eax});
                    break;
                    case (sm::binary_op::disjunction):
                    code_append_instruction(code, "orl", {rhs, reg::eax});
                    break;
                    default:
                    abort();
                }
                /*
                 * "any non-zero value is treated as truth and zero as falsity,
                 * AND THE RESULT RESPECTS THIS CONVENTION". That is, we can safely
                 * return the result as is, without converting it to 0 or 1.
                 */
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
                code_append_instruction(code, "idivl", {rhs});
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
                code_append_instruction(code, "idivl", {rhs});
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
