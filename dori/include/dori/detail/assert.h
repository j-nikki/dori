#pragma once

#include <assert.h>
#include <stdexcept>
#include <type_traits>

#ifndef NDEBUG
#define DORI_assert(Expr)                                                      \
    []() {                                                                     \
        if constexpr (std::is_constant_evaluated())                            \
            return [](bool e) {                                                \
                if (!e)                                                        \
                    throw std::runtime_error{#Expr " != true"};                \
            };                                                                 \
        else                                                                   \
            return [](bool e) { assert(e); };                                  \
    }()(Expr)
#else
#define DORI_assert(Expr) __assume(Expr)
#endif