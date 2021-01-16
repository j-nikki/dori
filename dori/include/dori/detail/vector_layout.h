#pragma once

#include "traits.h"
#include "vector_fwd.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <numeric>
#include <utility>

namespace dori::detail
{

template <class Al, class... Ts, std::size_t... Is>
constexpr auto Get_vector(std::index_sequence<Is...>)
{
    constexpr auto res = [] {
        std::array xs{std::tuple{static_cast<std::ptrdiff_t>(sizeof(Ts)),
                                 Get_type_name<Ts>(), Is}...};
        std::sort(xs.begin(), xs.end(), std::greater<>{});

        std::array idx{std::get<2>(xs[Is])...};
        std::array<std::size_t, xs.size()> offs{}, redir{};
        (..., (redir[idx[Is]] = Is));
        std::ptrdiff_t off = 0;
        (..., (offs[Is] = off, off += std::get<0>(xs[Is])));

        return std::array{idx, offs, redir};
    }();
    using Ts_   = Types<Ts...>;
    using TsSrt = Types<Ts_::template Ith_t<res[0][Is]>...>;
    return vector_impl<Al, Ts_, TsSrt, res[1], res[2], Is...>{};
}

template <class Al, class... Ts>
using Get_vector_t =
    decltype(Get_vector<Al, Ts...>(std::index_sequence_for<Ts...>{}));

} // namespace dori::detail