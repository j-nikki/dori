#pragma once

#include "inline.h"
#include "traits.h"
#include "vector_fwd.h"
#include "vector_layout.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string_view>
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
    std::byte *p_    = nullptr;
    std::size_t sz_  = 0;
    std::size_t cap_ = 0;
};

//
// Destroying up to a given element is an operation used in unhappy paths and is
// thus not important to be inlined. Externalizing this functionality allows us
// to engage the same machinery for vector<i32, i64> and vector<i64, i32> (as
// their underlying layouts match).
//

template <std::size_t... Ns>
struct Destroy_to_impl {
    template <class Al, class... Ts>
    static void fn(opaque_vector<Al> &v, void *p, std::size_t f_idx) noexcept
    {
        (... && [&]<class T>(auto N) {
            const auto data = reinterpret_cast<T *>(v.p_ + N * v.cap_);
            const auto p_   = reinterpret_cast<T *>(p);
            const auto last = data + v.sz_;
            const auto lt   = std::less<>{}(p_, last);
            const auto l    = lt ? p_ : last;
            for (auto it = data + f_idx; it != l; ++it)
                Call_maybe_unsafe(
                    DORI_f_ref(std::allocator_traits<Al>::destroy), v.al_, it);
            return lt;
        }.template operator()<Ts>(Ns));
    }
};

} // namespace dori::detail