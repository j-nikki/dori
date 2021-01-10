#pragma once

#include "inline.h"
#include "vector_fwd.h"

#include <algorithm>
#include <array>
#include <memory>
#include <utility>

namespace dori::detail
{

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
#define DORI_no_unique_address                                                 \
    __pragma(warning(push)) __pragma(warning(disable : 4648))                  \
        [[no_unique_address]] __pragma(warning(pop))
#else
#define DORI_no_unique_address [[no_unique_address]]
#endif

template <class Allocator>
struct opaque_vector {
    DORI_no_unique_address Allocator al_;
    char *p_         = nullptr;
    std::size_t sz_  = 0;
    std::size_t cap_ = 0;
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
        (..., [&]<std::ptrdiff_t N, class T>() {
            const auto data = reinterpret_cast<T *>(v.p_ + N * v.cap_);
            const auto l = std::min(reinterpret_cast<uintptr_t>(data + v.sz_),
                                    reinterpret_cast<uintptr_t>(p));
            for (auto it = data + f_idx; reinterpret_cast<uintptr_t>(it) < l;)
                std::allocator_traits<Al>::destroy(v.al_, it++);
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