#pragma once

#include "cstddef"
#include "string"

/**
 * Returns the length of the longest prefix of src[pos..] accepted by the
 * regex, or -1 if no prefix matches.
 */
int
match(const std::string &regex, const std::string &src, size_t pos);
