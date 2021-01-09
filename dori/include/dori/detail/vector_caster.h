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

        auto sorted = [](auto... sz) {
            std::array arr{sz...};
            std::sort(arr.begin(), arr.end());
            return arr;
        };
        static_assert(sorted(sizeof(Ts)...) == sorted(sizeof(Us)...),
                      "type sizes of given vectors must match");

        return *reinterpret_cast<vector_al<Al, Ts...> *>(&src);
    }
};

} // namespace dori::detail