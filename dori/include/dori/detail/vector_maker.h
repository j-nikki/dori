#pragma once

#include "inline.h"
#include "vector_fwd.h"

#include <algorithm>
#include <boost/align/aligned_allocator.hpp>
#include <memory>
#include <type_traits>

namespace dori::detail
{
template <class T>
concept Char_allocator =
    std::is_same_v<typename std::allocator_traits<T>::value_type, char>;
template <class... Ts>
struct vector_maker {
    template <Char_allocator Al>
    DORI_inline vector_al<Al, Ts...> operator()(const Al &al) const
        noexcept(noexcept(Al{al}))
    {
        return al;
    }
    DORI_inline vector<std::index_sequence_for<Ts...>, Ts...>
    operator()() const noexcept
    {
        return {};
    }
};
} // namespace dori::detail