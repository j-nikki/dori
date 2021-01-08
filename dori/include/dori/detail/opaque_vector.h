#pragma once

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

template <class...>
struct Destroy_to;
template <std::size_t... Is, class... Ts>
struct Destroy_to<std::index_sequence<Is...>, Ts...> {
    template <class Al>
    static void fn(opaque_vector<Al> &v, void *p) noexcept
    {
        (... && [&]<std::size_t I, class T>() {
            const auto data = reinterpret_cast<T *>(v.p_ + I * v.cap_);
            auto f          = data;
            const auto l    = std::min(data + v.sz_, reinterpret_cast<T *>(p));
            while (f != l)
                std::allocator_traits<Al>::destroy(v.al_, f++);
            return f == data + v.sz_;
        }.template operator()<Is, Ts>());
    }
};

} // namespace dori::detail