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
    template <class Al, std::size_t... Is, class... Us>
    constexpr DORI_inline auto &
    operator()(vector_impl<Al, std::index_sequence<Is...>, Us...> &src) const
    {
        using Res = vector_impl<Al, std::index_sequence<Is...>, Ts...>;
        using Src = std::remove_reference_t<decltype(src)>;

        static_assert(sizeof...(Ts) == sizeof...(Us),
                      "vector dimensions must match");

        constexpr bool equal_type_sizes = [] {
            std::array sz1{sizeof(Ts)...};
            std::array sz2{sizeof(Us)...};
            std::sort(sz1.begin(), sz1.end(), std::greater<>{});
            std::sort(sz2.begin(), sz2.end(), std::greater<>{});
            return std::equal(sz1.begin(), sz1.end(), sz2.begin(), sz2.end());
        }();
        static_assert(equal_type_sizes,
                      "type sizes of given vectors must match");

        return *reinterpret_cast<Res *>(&src);
    }
};

} // namespace dori::detail