#include "../src/regexp.h"

#include "cstdio"
#include "cstring"
#include "stdexcept"

static int failures = 0;

static void check(const char *regex, const char *src, size_t pos, int expected) {
    int got = match(regex, src, pos);
    if (got != expected) {
        printf("FAIL: match(\"%s\", \"%s\", %zu) = %d, expected %d\n",
               regex, src, pos, got, expected);
        ++failures;
    }
}

static void check_throws(const char *regex, const char *msg_fragment = nullptr) {
    try {
        match(regex, "", 0);
        printf("FAIL: match(\"%s\") should have thrown\n", regex);
        ++failures;
    } catch (const std::invalid_argument &e) {
        if (msg_fragment && !strstr(e.what(), msg_fragment)) {
            printf("FAIL: match(\"%s\") threw \"%s\", expected to contain \"%s\"\n",
                   regex, e.what(), msg_fragment);
            ++failures;
        }
    }
}

int main() {
    check("a",    "a",    0, 1);
    check("a",    "b",    0, -1);
    check("a",    "ab",   1, -1);

    check("ab",   "ab",   0, 2);
    check("ab",   "abc",  0, 2);
    check("ab",   "xab",  1, 2);
    check("abc",  "abc",  0, 3);

    check("a|b",  "a",    0, 1);
    check("a|b",  "b",    0, 1);
    check("a|b",  "c",    0, -1);

    check("a*",   "",     0, 0);
    check("a*",   "aaa",  0, 3);
    check("a*",   "b",    0, 0);
    check("(ab)*","ababab",0, 6);
    check("(ab)*","abx",  0, 2);

    check(".",    "a",    0, 1);
    check("..",   "ab",   0, 2);
    check("a.b",  "axb",  0, 3);
    check("a.b",  "ab",   0, -1);

    check("\\*",  "*",    0, 1);
    check("\\*",  "a",    0, -1);
    check("\\(",  "(",    0, 1);
    check("\\|",  "|",    0, 1);

    // ^ matches any single char NOT in the set
    check("^ab",  "c",    0, 1);
    check("^ab",  "a",    0, -1);
    check("^ab",  "b",    0, -1);
    check("^ab",  "x",    0, 1);
    check("^a",   "b",    0, 1);

    check("(^ab)*","cxyz", 0, 4);
    check("(^ab)*","cx",   0, 2);
    check("(^ab)*","axyz", 0, 0); // stops at 'a' which is excluded

    check("a^bc",  "ad",  0, 2);
    check("a^bc",  "ab",  0, -1);
    check("a^bc",  "ax",  0, 2);

    check("((a|b))", "a", 0, 1);
    check("((a|b))", "b", 0, 1);
    check("((a|b))", "c", 0, -1);
    check("((a)*)",  "aaa",0, 3);

    check("(a|b)*c", "ababc", 0, 5);
    check("(a|b)*c", "c",     0, 1);
    check("(a|b)*c", "x",     0, -1);

    check_throws("(a",   "unbalanced");
    check_throws("a)",   "unbalanced");
    check_throws("((a)", "unbalanced");
    check_throws("(a))", "unbalanced");

    check_throws("*",  "invalid");
    check_throws("**", "empty"); // second '*' operand is not a literal
    check_throws("^",  "invalid");

    if (failures == 0)
        puts("All tests passed.");
    else
        printf("%d test(s) failed.\n", failures);
    return failures ? 1 : 0;
}
