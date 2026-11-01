#include "ast.h"
#include "grammar.h"
#include "lexer.h"
#include "parser.h"
#include "sm.h"
#include "x86.h"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <spawn.h>
#include <sys/wait.h>

extern char **environ;

#include "iostream"

static const char *term_name(enum terminal t) {
    switch (t) {
        case VAR:        return "var";
        case READ:       return "read";
        case WRITE:      return "write";
        case SKIP:       return "skip";
        case TRUE:       return "true";
        case FALSE:      return "false";
        case LIDENT:     return "lident";
        case UIDENT:     return "uident";
        case DECIMAL:    return "decimal";
        case ASSIGN:     return ":=";
        case DISJUNCTION:return "!!";
        case CONJUNCTION:return "&&";
        case EQ:         return "==";
        case NEQ:        return "!=";
        case LEQ:        return "<=";
        case LE:         return "<";
        case GEQ:        return ">=";
        case GE:         return ">";
        case PLUS:       return "+";
        case MINUS:      return "-";
        case MUL:        return "*";
        case DIV:        return "/";
        case MOD:        return "%";
        case SEMICOLON:  return ";";
        case COMMA:      return ",";
        case LBRACE:     return "(";
        case RBRACE:     return ")";
        default:         return "?";
    }
}

static void print_tokens(const std::vector<token> &tokens) {
    std::cout << "tokens:";
    for (const auto &t : tokens)
        std::cout << " " << term_name(t.type);
    std::cout << "\n";
}

static void dump(const node_ptr &node, int depth) {
    for (int i = 0; i < depth; i++)
        std::cout << "  ";

    if (auto p = dynamic_cast<token_node *>(node.get())) {
        std::cout << "token:" << term_name(p->tok.type) << "\n";
        return;
    }
    if (auto p = dynamic_cast<number_literal *>(node.get())) {
        std::cout << "number:" << p->num << "\n";
        return;
    }
    if (auto p = dynamic_cast<binexpr *>(node.get())) {
        std::cout << "binexpr:" << term_name(p->op) << "\n";
        dump(p->lhs, depth + 1);
        dump(p->rhs, depth + 1);
        return;
    }
    if (auto p = dynamic_cast<unary_neg *>(node.get())) {
        std::cout << "unary_neg\n";
        dump(p->operand, depth + 1);
        return;
    }
    if (auto p = dynamic_cast<builtin_call *>(node.get())) {
        std::cout << "builtin:" << term_name(p->name.type) << "\n";
        if (p->arg) dump(p->arg, depth + 1);
        return;
    }
    if (auto p = dynamic_cast<var_def_item_node *>(node.get())) {
        std::cout << "var_def_item:" << p->name.text << "\n";
        if (p->init) dump(p->init, depth + 1);
        return;
    }
    if (auto p = dynamic_cast<var_def_node *>(node.get())) {
        std::cout << "var_def\n";
        dump(p->seq, depth + 1);
        return;
    }
    if (auto p = dynamic_cast<scope_expr_node *>(node.get())) {
        std::cout << "scope\n";
        if (p->defs) dump(p->defs, depth + 1);
        if (p->expr) dump(p->expr, depth + 1);
        return;
    }
    if (auto p = dynamic_cast<seq_node *>(node.get())) {
        std::cout << "seq\n";
        for (auto &item : p->items) dump(item, depth + 1);
        return;
    }
    std::cout << "<unknown node>\n";
}

static int
link_program(const std::string &assembly, const std::string &output)
{
    const char *runtime_dir = std::getenv("LAMA_RUNTIME");
    const auto runtime = (std::filesystem::path(
        runtime_dir ? runtime_dir : "runtime") / "runtime.a").string();
    const char *args[] = {
        "gcc", "-g", "-m32", "-no-pie", "-o", output.c_str(),
        assembly.c_str(),
        runtime.c_str(),
        nullptr
    };
    pid_t pid;
    int error = posix_spawnp(&pid, "gcc", nullptr, nullptr,
                             const_cast<char **>(args), environ);
    if (error != 0) {
        std::cerr << "gcc: " << std::strerror(error) << '\n';
        return 1;
    }
    int status;
    while (waitpid(pid, &status, 0) == -1) {
        if (errno != EINTR) {
            std::cerr << "waitpid: " << std::strerror(errno) << '\n';
            return 1;
        }
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

int main(int argc, char **argv) try {
    const std::string command = argc > 1 ? argv[1] : "";
    if (!((command == "tokens" || command == "ast") && argc == 3) &&
        !(command == "compile" && (argc == 3 || argc == 4))) {
        std::cerr << "usage:\n"
                  << "  " << argv[0] << " tokens <source.lama>\n"
                  << "  " << argv[0] << " ast <source.lama>\n"
                  << "  " << argv[0] << " compile <source.lama> [output]\n";
        return 1;
    }
    const auto input = std::filesystem::path(argv[2]);
    std::ifstream source(input);
    if (!source) {
        std::cerr << "cannot open source: " << input << '\n';
        return 1;
    }
    std::ostringstream contents;
    contents << source.rdbuf();
    if (source.bad()) {
        std::cerr << "cannot read source: " << input << '\n';
        return 1;
    }
    const auto src = contents.str();
    std::vector<token> tokens = lex(src);
    if (command == "tokens") {
        print_tokens(tokens);
        return 0;
    }

    node_ptr ast = parse(tokens);
    if (!ast) {
        std::cerr << "parse: no derivation\n";
        return 1;
    }
    if (command == "ast") {
        dump(ast, 0);
        return 0;
    }
    assert(command == "compile");
    auto output_path = input;
    if (argc == 4)
        output_path = argv[3];
    else if (output_path.has_extension())
        output_path.replace_extension();
    else
        output_path += ".out";
    const auto output = std::filesystem::absolute(output_path).string();
    const auto assembly = output + ".s";
    for (const auto &path : {output, assembly}) {
        if (std::filesystem::weakly_canonical(path) ==
                std::filesystem::weakly_canonical(input) ||
            (std::filesystem::exists(path) && std::filesystem::equivalent(path, input))) {
            std::cerr << "output would overwrite source: " << path << '\n';
            return 1;
        }
    }
    const auto code = x86::compile(sm::compile(*ast));
    std::ofstream file(assembly);
    file << code;
    file.close();
    if (!file) {
        std::cerr << "cannot write assembly: " << assembly << '\n';
        return 1;
    }
    return link_program(assembly, output);
} catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
}
