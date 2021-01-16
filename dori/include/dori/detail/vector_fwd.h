#pragma once

#include <boost/align/aligned_allocator.hpp>

namespace dori
{

template <class Allocator, class... Ts>
struct vector_al;

namespace detail
{
template <class, class, class, auto, auto, std::size_t...>
class vector_impl;
}
// template <class... Ts>
// using vector = vector_al<
//    boost::alignment::aligned_allocator<char, std::max({alignof(Ts)...})>,
//    Ts...>;
//}

} // namespace dori