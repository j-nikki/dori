#pragma once

#include <boost/mp11/algorithm.hpp>
#include <iterator>
#include <type_traits>

namespace dori::detail
{

using namespace boost::mp11;

template <class For, class T>
concept Init_iter =
    std::is_constructible_v<For, typename std::iterator_traits<T>::value_type>;

template <class For, class T>
concept Allocator = requires(T &al)
{
    typename T::pointer;
    {
        al.allocate(0)
    }
    ->std::same_as<typename T::pointer>;
}
|| requires(T &al)
{
    {
        al.allocate(0)
    }
    ->std::same_as<typename T::value_type *>;
};

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

template <class T>
static constexpr auto Get_type_name()
{
    std::string_view sv = __FUNCSIG__;
    auto f_i            = sv.find('<');
    auto l_i            = sv.rfind('>');
    return sv.substr(f_i, l_i - f_i);
}

} // namespace dori::detail