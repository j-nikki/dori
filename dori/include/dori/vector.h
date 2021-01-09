#pragma once

#include "detail/assert.h"
#include "detail/opaque_vector.h"
#include "detail/traits.h"
#include "detail/vector_caster.h"
#include "detail/vector_creator.h"

#include <algorithm>
#include <array>
#include <boost/align/aligned_allocator.hpp>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>

namespace dori
{

namespace detail
{

template <class T>
concept Tuple = requires
{
    typename std::tuple_size<T>;
};

template <class T>
using Move_t =
    std::conditional_t<std::is_trivially_copy_constructible_v<T>, T &, T &&>;

template <class Al, std::size_t... Is, class... Ts>
class vector_impl<Al, std::index_sequence<Is...>, Ts...> : opaque_vector<Al>
{
    using opaque_vector<Al>::al_;
    using opaque_vector<Al>::p_;
    using opaque_vector<Al>::sz_;
    using opaque_vector<Al>::cap_;
    using Al_tr = std::allocator_traits<Al>;

    template <std::size_t I>
    using Elem = std::tuple_element_t<I, std::tuple<Ts...>>;

    static constexpr auto Sz_all = (sizeof(Ts) + ...);
    static constexpr auto Align  = std::max({alignof(Ts)...});

#define DORI_VECTOR_ITERATOR_CONVOP_REFCONV_AND_PTRSTY_const_iterator          \
    std::tuple<const Ts *...>
#define DORI_VECTOR_ITERATOR_CONVOP_REFCONV_AND_PTRSTY_iterator                \
    constexpr DORI_inline operator const_iterator() const noexcept             \
    {                                                                          \
        return {ptrs, i};                                                      \
    }                                                                          \
    std::tuple<Ts *...>

    struct End_iterator;
#define DORI_VECTOR_ITERATOR(It, Ref)                                          \
    struct It {                                                                \
        using difference_type   = std::ptrdiff_t;                              \
        using value_type        = vector_impl::value_type;                     \
        using reference         = vector_impl::reference;                      \
        using iterator_category = std::input_iterator_tag;                     \
        constexpr DORI_inline It &operator++() noexcept { return ++i, *this; } \
        constexpr DORI_inline It operator++(int) noexcept                      \
        {                                                                      \
            return {ptrs, i++};                                                \
        }                                                                      \
        constexpr DORI_inline Ref operator*() noexcept                         \
        {                                                                      \
            return {std::get<Is>(ptrs)[i]...};                                 \
        }                                                                      \
        constexpr DORI_inline bool operator==(const It &it) const noexcept     \
        {                                                                      \
            return i == it.i;                                                  \
        }                                                                      \
        constexpr DORI_inline bool operator!=(const It &it) const noexcept     \
        {                                                                      \
            return i != it.i;                                                  \
        }                                                                      \
        constexpr DORI_inline bool operator==(End_iterator) const noexcept     \
        {                                                                      \
            return i == 0;                                                     \
        }                                                                      \
        constexpr DORI_inline bool operator!=(End_iterator) const noexcept     \
        {                                                                      \
            return i != 0;                                                     \
        }                                                                      \
        DORI_VECTOR_ITERATOR_CONVOP_REFCONV_AND_PTRSTY_##It ptrs;              \
        std::ptrdiff_t i;                                                      \
    };

  public:
    using value_type      = std::tuple<Ts...>;
    using reference       = std::tuple<Ts &...>;
    using const_reference = std::tuple<const Ts &...>;
    using allocator_type  = Al;
    DORI_VECTOR_ITERATOR(const_iterator, const_reference)
    DORI_VECTOR_ITERATOR(iterator, reference)

  private:
    struct End_iterator {
        constexpr DORI_inline operator iterator() const noexcept
        {
            return {.i = 0};
        }
        constexpr DORI_inline operator const_iterator() const noexcept
        {
            return {.i = 0};
        }
    };

  public:
    //
    // Member functions
    //

    constexpr DORI_inline vector_impl() noexcept : opaque_vector<Al>{} {}
    constexpr DORI_inline
    vector_impl(const Al &al) noexcept(noexcept(opaque_vector<Al>{al}))
        : opaque_vector<Al>{al}
    {
    }
    constexpr DORI_inline vector_impl(vector_impl &&other) noexcept
        : opaque_vector<Al>{std::move(other.al_), other.p_, other.sz_,
                            other.cap_}
    {
        other.p_   = nullptr;
        other.sz_  = 0;
        other.cap_ = 0;
    }
    constexpr DORI_inline vector_impl(const vector_impl &other)
        : opaque_vector<Al>{other}
    {
        p_ = Allocate(cap_ * Sz_all);
        (..., [&]<std::size_t I, class T>(const T *f, T *d_f) {
            try {
                for (const auto l = f + sz_; f != l; ++f, ++d_f)
                    Al_tr::construct(al_, d_f, f[0]);
            } catch (...) {
                Destroy_to(d_f);
                Al_tr::deallocate(al_, p_, cap_ * Sz_all);
                throw;
            }
        }.template operator()<Is>(other.data<Is>(), data<Is>()));
    }

    constexpr DORI_inline vector_impl &operator=(const vector_impl &rhs)
    {
        vector_impl{rhs}.swap(*this);
        return *this;
    }

    constexpr DORI_inline vector_impl &operator=(vector_impl &&rhs) noexcept
    {
        vector_impl{std::move(rhs)}.swap(*this);
        return *this;
    }

    constexpr DORI_inline ~vector_impl()
    {
        if (p_) {
            (..., [&]<class T>(T *f, T *l) {
                while (f != l)
                    Al_tr::destroy(al_, f++);
            }(data<Is>(), data<Is>() + sz_));
            Al_tr::deallocate(al_, p_, cap_ * Sz_all);
        }
    }

    constexpr DORI_inline Al get_allocator() const noexcept { return al_; }

    //
    // Element access
    //

    constexpr DORI_inline reference operator[](std::size_t i) noexcept
    {
        DORI_assert(i < sz_);
        return {reinterpret_cast<Ts *>(&p_[Offsets[Is] * cap_])[i]...};
    }

    constexpr DORI_inline const_reference
    operator[](std::size_t i) const noexcept
    {
        DORI_assert(i < sz_);
        return {reinterpret_cast<const Ts *>(&p_[Offsets[Is] * cap_])[i]...};
    }

    constexpr DORI_inline reference at(std::size_t i)
    {
        if (i >= sz_)
            throw std::out_of_range{"dori::vector::at"};
        return operator[](i);
    }

    constexpr DORI_inline const_reference at(std::size_t i) const
    {
        if (i >= sz_)
            throw std::out_of_range{"dori::vector::at"};
        return operator[](i);
    }

    constexpr DORI_inline reference front() noexcept
    {
        DORI_assert(sz_ > 0);
        return *begin();
    }
    constexpr DORI_inline const_reference front() const
    {
        DORI_assert(sz_ > 0);
        return *begin();
    }

    constexpr DORI_inline reference back()
    {
        DORI_assert(sz_ > 0);
        return *operator[](sz_ - 1);
    }
    constexpr DORI_inline const_reference back() const
    {
        DORI_assert(sz_ > 0);
        return *operator[](sz_ - 1);
    }

    template <std::size_t I>
    requires(I < sizeof...(Ts)) //
        constexpr DORI_inline auto data() noexcept
    {
        return reinterpret_cast<Elem<I> *>(p_ + Offsets[I] * cap_);
    }

    template <std::size_t I>
    requires(I < sizeof...(Ts)) //
        constexpr DORI_inline auto data() const noexcept
    {
        return reinterpret_cast<const Elem<I> *>(p_ + Offsets[I] * cap_);
    }

    //
    // Iterators
    //

    constexpr DORI_inline iterator begin() noexcept
    {
        return {{(data<Is>() + sz_)...}, -(intptr_t)sz_};
    }
    constexpr DORI_inline const_iterator begin() const noexcept
    {
        return {{(data<Is>() + sz_)...}, -(intptr_t)sz_};
    }
    constexpr DORI_inline const_iterator cbegin() const noexcept
    {
        return begin();
    }

    constexpr DORI_inline End_iterator cend() const noexcept { return {}; }
    constexpr DORI_inline End_iterator end() const noexcept { return {}; }

    //
    // Capacity
    //

    constexpr DORI_inline std::size_t size() const noexcept { return sz_; }
    constexpr DORI_inline std::size_t capacity() const noexcept { return cap_; }
    constexpr DORI_inline bool empty() const noexcept { return !sz_; }

    constexpr DORI_inline void reserve(std::size_t cap)
    {
        DORI_assert(cap > cap_);
        auto p = Allocate(cap * Sz_all);
        if (p_)
            Move_to_alloc(cap, p);
        p_   = p;
        cap_ = cap;
    }

    constexpr DORI_inline void resize(std::size_t sz)
    {
        if (sz > sz_) { // proposed exceeds current => extend
            (..., [&]<class T>(T *f, T *l) {
                while (f != l)
                    Al_tr::construct(al_, f++);
            }(data<Is>() + sz_, data<Is>() + sz));
        } else { // current exceeds proposed => shrink
            (..., [&]<class T>(T *f, T *l) {
                while (f != l)
                    Al_tr::destroy(al_, f++);
            }(data<Is>() + sz, data<Is>() + sz_));
        }
        sz_ = sz;
    }

    constexpr DORI_inline void swap(vector_impl &other) noexcept
    {
        std::swap(p_, other.p_);
        std::swap(sz_, other.sz_);
        std::swap(cap_, other.cap_);
    }

    //
    // Modifiers
    //

    constexpr DORI_inline iterator erase(
        const_iterator first,
        const_iterator last) noexcept((std::is_nothrow_move_assignable_v<Ts> &&
                                       ...))
    {
        const auto f_i = sz_ + first.i;
        const auto n   = last.i - first.i;
        const std::tuple ptrs{(&data<Is>()[sz_] + f_i)...};
        (..., [&](auto d_f) {
            const auto e = d_f - first.i;
            for (auto f = d_f + n; f != e; ++f, ++d_f)
                d_f[0] = std::move(f[0]);
            while (d_f != e)
                Al_tr::destroy(al_, d_f++);
        }(std::get<Is>(ptrs)));
        return {{std::get<Is>(ptrs)...}, -static_cast<std::intptr_t>(sz_ -= n)};
    }

    constexpr DORI_inline iterator
    erase(const_iterator pos) noexcept(noexcept(erase(pos, std::next(pos))))
    {
        return erase(pos, std::next(pos));
    }

    template <class... Us>
    requires((std::is_constructible_v<Ts, Us &&> && ...) &&
             sizeof...(Us) == sizeof...(Ts)) //
        constexpr DORI_inline void push_back(Us &&...xs) noexcept(noexcept(
            emplace_back(std::piecewise_construct,
                         std::tuple<Us &&>{static_cast<Us &&>(xs)}...)))
    {
        emplace_back(std::piecewise_construct,
                     std::tuple<Us &&>{static_cast<Us &&>(xs)}...);
    }

    constexpr DORI_inline void push_back(const value_type &value) noexcept(
        noexcept(push_back(std::get<Is>(value)...)))
    {
        push_back(std::get<Is>(value)...);
    }

    constexpr DORI_inline void push_back(value_type &&value) noexcept(
        noexcept(push_back(std::get<Is>(static_cast<value_type &&>(value))...)))
    {
        push_back(std::get<Is>(static_cast<value_type &&>(value))...);
    }

    template <class... Us>
    requires((std::is_constructible_v<Ts, Us &&> && ...) &&
             sizeof...(Us) == sizeof...(Ts)) //
        constexpr DORI_inline iterator
        emplace_back(Us &&...xs) noexcept(noexcept(
            emplace_back(std::piecewise_construct,
                         std::tuple<Us &&>{static_cast<Us &&>(xs)}...)))
    {
        return emplace_back(std::piecewise_construct,
                            std::tuple<Us &&>{static_cast<Us &&>(xs)}...);
    }

    template <Tuple... Us>
    requires(sizeof...(Ts) == sizeof...(Us)) //
        constexpr DORI_inline iterator
        emplace_back(std::piecewise_construct_t, Us &&...xs) noexcept(
            (... && (noexcept(Emplace(al_, std::declval<Ts *>(),
                                      std::declval<Us &&>())))))
    {
        DORI_assert(sz_ < cap_);
        const std::array datas{(p_ + Offsets[Is] * cap_)...};
        const std::tuple ptrs{&reinterpret_cast<Ts *>(datas[Is])[sz_]...};
        (Emplace(al_, std::get<Is>(ptrs), static_cast<Us &&>(xs)), ...);
        return {{std::get<Is>(ptrs)...}, -static_cast<intptr_t>(sz_++)};
    }

  private:
    //
    // Modification
    //

    static constexpr auto Emplace = []<class T>(auto &al, const auto p, T &&t) {
        [&]<std::size_t... Js>(T && t, std::index_sequence<Js...>)
        {
            static_assert(
                DORI_f_ok(Al_tr::construct, al_, p,
                          std::get<Js>(static_cast<T &&>(t))...),
                "elements not constructible with parameters to emplace()");
            Al_tr::construct(al, p, std::get<Js>(static_cast<T &&>(t))...);
        }
        (static_cast<T &&>(t),
         std::make_index_sequence<std::tuple_size_v<T>>{});
    };

    //
    // Allocation
    //

    constexpr DORI_inline void Move_to_alloc(std::size_t cap, auto p)
    {
        DORI_assert(cap >= sz_);
        (..., [&]<class T>(T *f, T *d_f) {
            for (const auto l = f + sz_; f != l; ++f, ++d_f) {
#ifndef NDEBUG
                try {
#endif
                    Al_tr::construct(al_, d_f, static_cast<Move_t<T>>(f[0]));
#ifndef NDEBUG
                } catch (...) {
                    DORI_assert(
                        !"disastrous error - move constrution shan't throw");
                    throw;
                }
#endif
                Al_tr::destroy(al_, f);
            }
        }(data<Is>(), reinterpret_cast<Elem<Is> *>(p + Offsets[Is] * cap)));
        Al_tr::deallocate(al_, p_, cap_ * Sz_all);
    }

    constexpr DORI_inline auto Allocate(std::size_t n)
    {
        DORI_assert(!(n % Sz_all));
        auto p = Al_tr::allocate(al_, n);
        DORI_assert(!(reinterpret_cast<uintptr_t>(p) % Align));
        return p;
    }

    constexpr DORI_inline void Destroy_to(void *p) noexcept
    {
        ::dori::detail::Destroy_to<
            std::index_sequence<static_cast<std::size_t>(Offsets[Is])...>,
            Elem<Is>...>::fn(*this, p);
    }

    //
    // Layout query
    //

    static constexpr inline auto Offsets = [] {
        std::array xs{std::pair{Is, sizeof(Ts)}...};
        std::sort(xs.begin(), xs.end(),
                  [](auto a, auto b) { return a.second > b.second; });
        std::array<std::ptrdiff_t, sizeof...(Ts)> res{};
        std::size_t sz = 0;
        for (auto [a, b] : xs) {
            res[a] = static_cast<std::ptrdiff_t>(sz);
            sz += b;
        }
        return res;
    }();
};

} // namespace detail

template <class... Ts>
constexpr inline detail::vector_caster<Ts...> vector_cast{};
template <class... Ts>
constexpr detail::vector_creator<Ts...> make_vector{};

template <class Allocator, class... Ts>
struct vector_al
    : detail::vector_impl<Allocator, std::index_sequence_for<Ts...>, Ts...> {
    using detail::vector_impl<Allocator, std::index_sequence_for<Ts...>,
                              Ts...>::vector_impl;
};
template <class... Ts>
using vector = detail::vector<Ts...>;

} // namespace dori