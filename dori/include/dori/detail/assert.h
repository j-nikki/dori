#pragma once

#include <stdexcept>
#include <type_traits>
#include <assert.h>

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
#define DORI_assert(...) ((void)0)
#endif