#pragma once

#include "mull/Mutators/MutatorKind.h"

#include <clang/AST/Expr.h>

#include <vector>

namespace mull {
extern std::vector<
    std::tuple<clang::BinaryOperator::Opcode, mull::MutatorKind, clang::BinaryOperator::Opcode>>
    BINARY_MUTATIONS;
extern std::vector<std::pair<clang::UnaryOperator::Opcode, mull::MutatorKind>> UNARY_MUTATIONS;
} // namespace mull
