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
    uint32_t index;
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

using instruction = std::variant<
    read,
    write,
    drop,
    binary, /* (s) */
    load,   /* (x) */
    store,  /* (x) */
    constant/* (n) */
>;

struct program {
    std::vector<instruction> code;
    uint32_t global_count = 0;
    // TODO: Reuse slots on scope exit, track peak locals per function.
    uint32_t local_count = 0;
};

} /* namespace sm */

sm::program
compile(const ast_node &root);
