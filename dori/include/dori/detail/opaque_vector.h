#pragma once

#include "inline.h"
#include "vector_fwd.h"

#include <algorithm>
#include <array>
#include <memory>
#include <utility>

namespace dori::detail
{

#define DORI_use_no_unique_address_begin                                       \
    __pragma(warning(push)) __pragma(warning(disable : 4648))
#define DORI_use_no_unique_address_end __pragma(warning(pop))

template <class Allocator>
struct opaque_vector {
    DORI_use_no_unique_address_begin [[no_unique_address]] Allocator al_;
    DORI_use_no_unique_address_end char *p_ = nullptr;
    std::size_t sz_                         = 0;
    std::size_t cap_                        = 0;
};

//
// Destroying up to a given element is an operation used in unhappy paths and is
// thus not important to be inlined. Externalizing this functionality allows us
// to engage the same machinery for vector<i32, i64> and vector<i64, i32> (as
// their underlying layouts match).
//

template <std::ptrdiff_t... Ns>
struct Destroy_tail_impl {
    template <class Al, class... Ts>
    static void fn(opaque_vector<Al> &v, void *p, std::size_t f_idx) noexcept
    {
        (... && [&]<std::ptrdiff_t N, class T>() {
            const auto data = reinterpret_cast<T *>(v.p_ + N * v.cap_);
            auto f          = data + f_idx;
            const auto l    = std::min(data + v.sz_, reinterpret_cast<T *>(p));
            while (f != l)
                std::allocator_traits<Al>::destroy(v.al_, f++);
            return f == data + v.sz_ + 1;
        }.template operator()<Ns, Ts>());
    }
};

struct Destroy_tail {
    template <class Al, std::size_t... Is, class... Ts>
    static constexpr DORI_inline void
    fn(vector_impl<Al, std::index_sequence<Is...>, Ts...> &v, void *p,
       std::size_t f_idx) noexcept
    {
        constexpr auto idx_sorted = [] {
            std::array xs{std::pair{sizeof(Ts), Is}...};
            std::sort(xs.begin(), xs.end());
            return std::array{xs[Is].second...};
        }();
        using V = vector_impl<Al, std::index_sequence<Is...>, Ts...>;
        Destroy_tail_impl<V::Offsets[idx_sorted[Is]]...>::template fn<
            Al, typename V::template Elem<idx_sorted[Is]>...>(v, p, f_idx);
    }
};

} // namespace dori::detail