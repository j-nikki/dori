#pragma once

#include "inline.h"
#include "vector_fwd.h"

#include <algorithm>
#include <array>
#include <type_traits>

namespace dori::detail
{

template <class... Ts>
struct vector_caster {
    template <class Al, class... Us>
    constexpr DORI_inline auto &operator()(vector_al<Al, Us...> &src) const
    {
        static_assert(sizeof...(Ts) == sizeof...(Us),
                      "vector dimensions must match");

        constexpr auto sz = [] {
            std::array sz_src{sizeof(Us)...};
            std::array sz_dst{sizeof(Ts)...};
            std::sort(sz_src.begin(), sz_src.end());
            std::sort(sz_dst.begin(), sz_dst.end());
            return std::array{sz_src, sz_dst};
        }();
        static_assert(sz[0] == sz[1], "type sizes of given vectors must match");
        static_assert(std::adjacent_find(sz[0].begin(), sz[0].end()) ==
                          sz[0].end(),
                      "no contained objects of equal size permitted in "
                      "vector_cast as their relative order is unspecified");

        return *reinterpret_cast<vector_al<Al, Ts...> *>(&src);
    }
};

} // namespace dori::detail