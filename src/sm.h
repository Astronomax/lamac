#pragma once

#include "ast.h"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace sm {

enum class location_kind {
    global,
    local,
    argument,
    captured
};

struct location {
    location_kind kind;
    std::variant<uint32_t, std::string> value;
};

struct global {
    std::string name;
};

struct read {};
struct write {};
struct drop {};

struct load {
    location source;
};

struct store {
    location destination;
};

enum class binary_op {
    plus,
    minus,
    mul,
    div,
    mod,
    le,
    leq,
    ge,
    geq,
    eq,
    neq,
    conjunction,
    disjunction
};

struct binary {
    binary_op op;
};

struct constant {
    int32_t value;
};

struct label {
    std::string label;
};

struct jmp {
    std::string label;
};

enum jump_condition {
    zero,
    not_zero,
};

/* Pops the condition on both the taken and fall-through paths. */
struct cjmp {
    jump_condition condition;
    std::string label;
};

struct begin {
    std::string label;
    size_t n_args;
    size_t n_locals;
};

struct end {};

struct call {
    std::string label;
    size_t n_args;
};

using instruction = std::variant<
    global,
    read,
    write,
    drop,
    binary,     /* (s) */
    load,       /* (x) */
    store,      /* (x) */
    constant,   /* (n) */
    label,
    jmp,
    cjmp,
    begin,
    end,
    call
>;

struct program {
    std::vector<instruction> code;
    uint32_t global_count = 0;
};

sm::program
compile(const ast_node &root);

} /* namespace sm */
