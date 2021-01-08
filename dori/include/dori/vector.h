#pragma once

#include "detail/opaque_vector.h"
#include "detail/vector_caster.h"
#include "detail/vector_creator.h"

#include <algorithm>
#include <array>
#include <assert.h>
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

    template <class, class = void>
    struct Is_tuple : std::false_type {
    };
    template <class T>
    struct Is_tuple<
        T, std::void_t<std::tuple_size<T>,
                       decltype(std::get<0>(std::declval<const T &>()))>>
        : std::bool_constant<std::tuple_size_v<T> == sizeof...(Ts)> {
    };

#define DORI_VECTOR_ITERATOR_CONVOP_REFCONV_AND_PTRSTY_const_iterator          \
    std::tuple<const Ts *...>
#define DORI_VECTOR_ITERATOR_CONVOP_REFCONV_AND_PTRSTY_iterator                \
    DORI_inline operator const_iterator() const noexcept { return {ptrs, i}; } \
    std::tuple<Ts *...>

    struct End_iterator;
#define DORI_VECTOR_ITERATOR(It, Ref)                                          \
    struct It {                                                                \
        using difference_type   = std::ptrdiff_t;                              \
        using value_type        = vector_impl::value_type;                     \
        using reference         = vector_impl::reference;                      \
        using iterator_category = std::input_iterator_tag;                     \
        DORI_inline It &operator++() noexcept { return ++i, *this; }           \
        DORI_inline It operator++(int) noexcept { return {ptrs, i++}; }        \
        DORI_inline Ref operator*() noexcept                                   \
        {                                                                      \
            return {std::get<Is>(ptrs)[i]...};                                 \
        }                                                                      \
        DORI_inline bool operator==(const It &it) const noexcept               \
        {                                                                      \
            return i == it.i;                                                  \
        }                                                                      \
        DORI_inline bool operator!=(const It &it) const noexcept               \
        {                                                                      \
            return i != it.i;                                                  \
        }                                                                      \
        DORI_inline bool operator==(End_iterator) const noexcept               \
        {                                                                      \
            return i == 0;                                                     \
        }                                                                      \
        DORI_inline bool operator!=(End_iterator) const noexcept               \
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
        DORI_inline operator iterator() const noexcept { return {.i = 0}; }
        DORI_inline operator const_iterator() const noexcept
        {
            return {.i = 0};
        }
    };

  public:
    //
    // Member functions
    //

    DORI_inline vector_impl() noexcept : opaque_vector<Al>{} {}
    DORI_inline
    vector_impl(const Al &al) noexcept(noexcept(opaque_vector<Al>{al}))
        : opaque_vector<Al>{al}
    {
    }
    DORI_inline vector_impl(vector_impl &&other) noexcept
        : opaque_vector<Al>{std::move(other.al_), other.p_, other.sz_,
                            other.cap_}
    {
        other.p_   = nullptr;
        other.sz_  = 0;
        other.cap_ = 0;
    }
    DORI_inline vector_impl(const vector_impl &other) : opaque_vector<Al>{other}
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

    DORI_inline vector_impl &operator=(const vector_impl &rhs)
    {
        vector_impl{rhs}.swap(*this);
        return *this;
    }

    DORI_inline vector_impl &operator=(vector_impl &&rhs) noexcept
    {
        vector_impl{std::move(rhs)}.swap(*this);
        return *this;
    }

    DORI_inline ~vector_impl()
    {
        if (p_) {
            (..., [&]<class T>(T *f, T *l) {
                while (f != l)
                    Al_tr::destroy(al_, f++);
            }(data<Is>(), data<Is>() + sz_));
            Al_tr::deallocate(al_, p_, cap_ * Sz_all);
        }
    }

    DORI_inline Al get_allocator() const noexcept { return al_; }

    //
    // Element access
    //

    DORI_inline reference operator[](std::size_t i) noexcept
    {
        assert(i < sz_);
        return {Ith_arr<Is>(p_, cap_)[i]...};
    }

    DORI_inline const_reference operator[](std::size_t i) const noexcept
    {
        assert(i < sz_);
        return {Ith_arr<Is>(p_, cap_)[i]...};
    }

    DORI_inline reference at(std::size_t i)
    {
        if (i >= sz_)
            throw std::out_of_range{"dori::vector::at"};
        return operator[](i);
    }

    DORI_inline const_reference at(std::size_t i) const
    {
        if (i >= sz_)
            throw std::out_of_range{"dori::vector::at"};
        return operator[](i);
    }

    DORI_inline reference front() noexcept
    {
        assert(sz_ > 0);
        return *begin();
    }
    DORI_inline const_reference front() const
    {
        assert(sz_ > 0);
        return *begin();
    }

    DORI_inline reference back()
    {
        assert(sz_ > 0);
        return *operator[](sz_ - 1);
    }
    DORI_inline const_reference back() const
    {
        assert(sz_ > 0);
        return *operator[](sz_ - 1);
    }

    template <std::size_t I>
    DORI_inline Elem<I> *data() noexcept
    {
        return Ith_arr<I>(p_, cap_);
    }

    template <std::size_t I>
    DORI_inline const Elem<I> *data() const noexcept
    {
        return Ith_arr<I>(p_, cap_);
    }

    //
    // Iterators
    //

    DORI_inline iterator begin() noexcept
    {
        return {{(data<Is>() + sz_)...}, -(intptr_t)sz_};
    }
    DORI_inline const_iterator begin() const noexcept
    {
        return {{(data<Is>() + sz_)...}, -(intptr_t)sz_};
    }
    DORI_inline const_iterator cbegin() const noexcept { return begin(); }

    DORI_inline End_iterator cend() const noexcept { return {}; }
    DORI_inline End_iterator end() const noexcept { return {}; }

    //
    // Capacity
    //

    DORI_inline std::size_t size() const noexcept { return sz_; }
    DORI_inline std::size_t capacity() const noexcept { return cap_; }
    DORI_inline bool empty() const noexcept { return !sz_; }

    DORI_inline void reserve(std::size_t cap)
    {
        if (cap <= cap_)
            return;
        if (!p_) {
            p_   = Allocate(cap * Sz_all);
            cap_ = cap;
        } else
            Re_alloc(cap);
    }

    DORI_inline void resize(std::size_t sz)
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

    DORI_inline void swap(vector_impl &other) noexcept
    {
        std::swap(p_, other.p_);
        std::swap(sz_, other.sz_);
        std::swap(cap_, other.cap_);
    }

    //
    // Modifiers
    //

    DORI_inline iterator erase(const_iterator first, const_iterator last)
    {
        const auto f_i = sz_ + first.i;
        const auto n   = last.i - first.i;
        const auto f   = [&]<class T>(T *d_f, T *l) {
            for (auto f = d_f + n; f != l; ++f, ++d_f)
                d_f[0] = std::move(f[0]);
            while (d_f != l)
                Al_tr::destroy(al_, d_f++);
        };
        return {{(f(data<Is>() + f_i, data<Is>() + sz_), data<Is>() + f_i)...},
                -static_cast<intptr_t>(sz_ -= n)};
    }

    DORI_inline iterator erase(const_iterator pos)
    {
        return erase(pos, std::next(pos));
    }

    template <class Tpl>
    requires Is_tuple<Tpl>::value //
        DORI_inline void
        push_back(Tpl &&xs) noexcept((std::is_nothrow_constructible_v<
                                          Ts, std::tuple_element_t<Is, Tpl>> &&
                                      ...))
    {
        push_back(std::get<Is>(static_cast<Tpl &&>(xs))...);
    }

    template <class... Us>
    requires(sizeof...(Us) == sizeof...(Ts)) //
        DORI_inline void push_back(Us &&...xs) noexcept(
            noexcept(Emplace_or_push_back<false>(static_cast<Us &&>(xs)...)))
    {
        Emplace_or_push_back<false>(static_cast<Us &&>(xs)...);
    }

    template <class... Us>
    requires(sizeof...(Ts) == sizeof...(Us) && (Is_tuple<Us>::value && ...)) //
        DORI_inline
        void emplace_back(std::piecewise_construct_t, Us &&...xs) noexcept(
            noexcept(Emplace_or_push_back<true>(static_cast<Us &&>(xs)...)))
    {
        Emplace_or_push_back<true>(static_cast<Us &&>(xs)...);
    }

    template <class... Us>
    requires(sizeof...(Ts) == sizeof...(Us)) //
        DORI_inline void emplace_back(Us &&...xs) noexcept(
            Emplace_or_push_back_is_noexcept<true, std::tuple<Us>...>::value)
    {
        emplace_back(std::piecewise_construct,
                     std::tuple<Us>{static_cast<Us &&>(xs)}...);
    }

  private:
    //
    // Modification
    //

    template <class, class Tuple,
              class = std::make_index_sequence<std::tuple_size_v<Tuple>>>
    struct Is_nothrow_emplaceable {
    };
    template <class T, class Tuple, std::size_t... Js>
    struct Is_nothrow_emplaceable<T, Tuple, std::index_sequence<Js...>>
        : std::is_nothrow_constructible<T, std::tuple_element_t<Js, Tuple>...> {
    };

    template <bool Emplace, class... Us>
    struct Emplace_or_push_back_is_noexcept
        : std::bool_constant<(
              Is_nothrow_emplaceable<Ts, std::decay_t<Us>>::value && ...)> {
    };
    template <class... Us>
    struct Emplace_or_push_back_is_noexcept<false, Us...>
        : std::bool_constant<(std::is_nothrow_constructible<Ts, Us>::value &&
                              ...)> {
    };

    template <bool Emplace, class... Us>
    DORI_inline void Emplace_or_push_back(Us &&...xs) noexcept(
        Emplace_or_push_back_is_noexcept<Emplace, Us...>::value)
    {
        assert(sz_ < cap_);
        (..., [&]<class T, class U>(T &dst, U &&x) {
            const auto addr = std::addressof(dst);
            assert((char *)addr >= p_);
            assert((char *)&addr[1] <= (char *)&p_[cap_ * Sz_all]);
            if constexpr (Emplace) {
                std::apply(
                    [&]<class... Vs>(Vs && ...xs) {
                        Al_tr::construct(al_, addr, static_cast<Vs &&>(xs)...);
                    },
                    static_cast<U &&>(x));
            } else
                Al_tr::construct(al_, addr, static_cast<U &&>(x));
        }(data<Is>()[sz_], static_cast<Us &&>(xs)));
        ++sz_;
    }

    //
    // Allocation
    //

    DORI_inline void Re_alloc(std::size_t cap)
    {
        assert(cap >= sz_);
        auto p = Allocate(cap * Sz_all);
        (..., [&]<class T>(T *f, T *d_f) {
            for (const auto l = f + sz_; f != l; ++f, ++d_f) {
#ifndef NDEBUG
                try {
#endif
                    Al_tr::construct(al_, d_f, static_cast<Move_t<T>>(f[0]));
#ifndef NDEBUG
                } catch (...) {
                    assert(!"disastrous error - move constrution shan't throw");
                    throw;
                }
#endif
                Al_tr::destroy(al_, f);
            }
        }(data<Is>(), Ith_arr<Is>(p, cap)));
        Al_tr::deallocate(al_, p_, cap_ * Sz_all);
        p_   = p;
        cap_ = cap;
    }

    DORI_inline auto Allocate(std::size_t n)
    {
        assert(!(n % Sz_all));
        auto p = Al_tr::allocate(al_, n);
        assert(!(reinterpret_cast<uintptr_t>(p) % Align));
        return p;
    }

    DORI_inline void Destroy_to(void *p) noexcept
    {
        static constexpr auto os = Get_offsets();
        using Os_is = std::index_sequence<static_cast<std::size_t>(os[Is])...>;
        ::dori::detail::Destroy_to<Os_is, Elem<Is>...>::fn(*this, p);
    }

    //
    // Layout query
    //

    template <std::size_t I, class T, class Rev = std::false_type>
    static constexpr auto Ith_arr(T *p, std::size_t cap, Rev = {}) noexcept
    {
        using E      = Elem<I>;
        using EConst = std::conditional_t<std::is_const_v<T>, const E, E>;
        static constexpr auto offset =
            Get_offsets()[I] + (Rev::value ? sizeof(E) : 0);
        return reinterpret_cast<EConst *>(&p[offset * cap]);
    }

    static constexpr auto Get_offsets() noexcept
    {
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
    }
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