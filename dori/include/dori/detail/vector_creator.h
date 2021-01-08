#pragma once

#include "inline.h"
#include "vector_fwd.h"

#include <algorithm>
#include <boost/align/aligned_allocator.hpp>
#include <memory>
#include <type_traits>

namespace dori::detail
{
template <class... Ts>
struct vector_creator {
    template <class Al>
    requires(
        requires { std::allocator_traits<Al>; } &&
        std::is_same_v<typename std::allocator_traits<Al>::value_type, char>) //
        DORI_inline vector_impl<Al, std::index_sequence_for<Ts...>, Ts...>
        operator()(const Al &al) const noexcept(noexcept(Al{al}))
    {
        return al;
    }
    DORI_inline vector_impl<
        boost::alignment::aligned_allocator<char, std::max({alignof(Ts)...})>,
        std::index_sequence_for<Ts...>, Ts...>
    operator()() const noexcept
    {
        return {};
    }
};
} // namespace dori::detail