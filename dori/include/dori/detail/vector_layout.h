#pragma once

#include <array>
#include <cstddef>
#include <utility>

namespace dori::detail
{
template <std::size_t... Is, std::size_t... Szs>
static constexpr auto Get_offsets(std::index_sequence<Is...>,
                                  std::index_sequence<Szs...>)
{
    return std::array{[](std::size_t I, std::size_t Sz) {
        return static_cast<std::ptrdiff_t>(
            (... + (Sz > Szs || Sz == Szs && I <= Is ? 0 : Szs)));
    }(Is, Szs)...};
}
template <class... Ts>
constexpr inline auto
    Offsets = Get_offsets(std::index_sequence_for<Ts...>{},
                          std::index_sequence<sizeof(Ts)...>{});
} // namespace dori::detail