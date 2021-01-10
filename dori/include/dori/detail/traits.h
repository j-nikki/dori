#pragma once

#include <type_traits>

namespace dori::detail
{
#define DORI_args(a, b) DORI_args_exp(a, b)
#define DORI_args_exp(a, b) a##b
#define DORI_f_args DORI_args(Args, __LINE__)
#define DORI_f_ref(f)                                                          \
    []<class... DORI_f_args>(DORI_f_args && ...args) noexcept(                 \
        noexcept(f(static_cast<DORI_f_args &&>(args)...)))                     \
        ->decltype(f(static_cast<DORI_f_args &&>(args)...))                    \
    {                                                                          \
        return f(static_cast<DORI_f_args &&>(args)...);                        \
    }

template <class F, class... Args>
constexpr std::is_invocable<F, Args &&...> F_ok(F &&, Args &&...) noexcept
{
    return {};
}

#define DORI_f_ok(f, ...)                                                      \
    decltype(::dori::detail::F_ok(DORI_f_ref(f), __VA_ARGS__))::value
} // namespace dori::detail