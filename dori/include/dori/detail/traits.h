#pragma once

#include <type_traits>

namespace dori::detail
{

#define DORI_cat(a, b) DORI_cat_exp(a, b)
#define DORI_cat_exp(a, b) a##b
#define DORI_f_args DORI_cat(Args, __LINE__)
#define DORI_f_ref(f)                                                          \
    [&]<class... DORI_f_args>(DORI_f_args && ...args) noexcept(                \
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

template <class T>
concept Tuple = requires
{
    typename std::tuple_size<T>;
};

template <class T>
using Move_t =
    std::conditional_t<std::is_trivially_copy_constructible_v<T>, T &, T &&>;

} // namespace dori::detail