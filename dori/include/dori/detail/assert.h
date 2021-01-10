#pragma once

#include <stdexcept>
#include <stdlib.h>
#include <type_traits>

#ifndef DORI_DEBUG
#ifdef NDEBUG
#define DORI_DEBUG 0
#else
#define DORI_DEBUG 1
#endif
#endif

#if DORI_DEBUG
#define DORI_assert(Expr)                                                      \
    []() {                                                                     \
        if constexpr (std::is_constant_evaluated())                            \
            return [](bool e) {                                                \
                if (!e)                                                        \
                    throw std::runtime_error{#Expr " != true"};                \
            };                                                                 \
        else                                                                   \
            return [](bool e) {                                                \
                if (!e) {                                                      \
                    fprintf(stderr, "%s\n", #Expr " != true");                 \
                    abort();                                                   \
                }                                                              \
            };                                                                 \
    }()(Expr)
#else
#define DORI_assert(Expr) __assume(Expr)
#endif