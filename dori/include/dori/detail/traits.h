#pragma once

#include <type_traits>

namespace dori::detail
{
#define DORI_f_ref(f)                                                          \
    []<class... Args>(Args && ...args) noexcept(                               \
        noexcept(f(static_cast<Args &&>(args)...)))                            \
        ->decltype(f(static_cast<Args &&>(args)...))                           \
    {                                                                          \
        return f(static_cast<Args &&>(args)...);                               \
    }

template <class F, class... Args>
constexpr std::is_invocable<F, Args &&...> F_ok(F &&, Args &&...) noexcept
{
    return {};
}

#define DORI_f_ok(f, ...)                                                      \
    decltype(::dori::detail::F_ok(DORI_f_ref(f), __VA_ARGS__))::value
} // namespace dori::detail