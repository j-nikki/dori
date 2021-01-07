﻿#pragma once

#include <algorithm>
#include <array>
#include <assert.h>
#include <boost/align/aligned_allocator.hpp>
#include <memory>
#include <tuple>

namespace dori
{

namespace detail
{

#define DORI_use_no_unique_address_begin                                       \
    __pragma(warning(push)) __pragma(warning(disable : 4648))
#define DORI_use_no_unique_address_end __pragma(warning(pop))

#define DORI_explicit_new_and_delete_begin                                     \
    __pragma(warning(push)) __pragma(warning(disable : 26409))
#define DORI_explicit_new_and_delete_end __pragma(warning(pop))

#define DORI_c_style_cast_begin                                                \
    __pragma(warning(push)) __pragma(warning(disable : 26493))
#define DORI_c_style_cast_end __pragma(warning(pop))

template <class, class, class...>
class vector_impl;

template <class... Ts>
struct vector_creator {
    template <class Allocator>
    requires std::is_same_v<typename Allocator::value_type, char> //
        inline vector_impl<Allocator, std::index_sequence_for<Ts...>, Ts...>
        operator()(const Allocator &al) const /*noexcept*/
    {
        return al;
    }
    inline vector_impl<
        boost::alignment::aligned_allocator<char, std::max({sizeof(Ts)...})>,
        std::index_sequence_for<Ts...>, Ts...>
    operator()() const noexcept
    {
        return {};
    }
};

template <class Al, std::size_t... Is, class... Ts>
class vector_impl<Al, std::index_sequence<Is...>, Ts...>
{
    using AlTr = std::allocator_traits<Al>;

    template <std::size_t I>
    using Elem = std::tuple_element_t<I, std::tuple<Ts...>>;

    static constexpr auto Sz_all = (sizeof(Ts) + ...);

    static constexpr std::array<std::ptrdiff_t, sizeof...(Ts)> Get_offsets()
    {
        std::array xs{
            std::pair<std::size_t, std::ptrdiff_t>{Is, sizeof(Ts)}...};
        std::sort(xs.begin(), xs.end(),
                  [](auto a, auto b) { return a.second > b.second; });
        std::array<std::ptrdiff_t, sizeof...(Ts)> res{};
        std::size_t sz = 0;
        for (auto [a, b] : xs) {
            res[a] = sz;
            sz += b;
        }
        return res;
    }

    template <class, class = void>
    struct Is_tuple : std::false_type {
    };
    template <class T>
    struct Is_tuple<
        T, std::void_t<std::tuple_size<T>,
                       decltype(std::get<0>(std::declval<const T &>()))>>
        : std::bool_constant<std::tuple_size_v<T> == sizeof...(Ts)> {
    };

#define SOA_VECTOR_ITERATOR_CONVOP_REFCONV_AND_PTRSTY_const_iterator           \
    std::tuple<const Ts *...>
#define SOA_VECTOR_ITERATOR_CONVOP_REFCONV_AND_PTRSTY_iterator                 \
    inline operator const_iterator() const noexcept { return {ptrs, i}; }      \
    std::tuple<Ts *...>

    struct End_iterator;
#define SOA_VECTOR_ITERATOR(It, Ref)                                           \
    struct It {                                                                \
        using difference_type   = std::ptrdiff_t;                              \
        using value_type        = vector_impl::value_type;                     \
        using reference         = vector_impl::reference;                      \
        using iterator_category = std::input_iterator_tag;                     \
        It &operator++() noexcept { return ++i, *this; }                       \
        It operator++(int) noexcept { return {ptrs, i++}; }                    \
        Ref operator*() noexcept { return {std::get<Is>(ptrs)[i]...}; }        \
        bool operator==(const It &it) const noexcept { return i == it.i; }     \
        bool operator!=(const It &it) const noexcept { return i != it.i; }     \
        bool operator==(End_iterator) const noexcept { return i == 0; }        \
        bool operator!=(End_iterator) const noexcept { return i != 0; }        \
        SOA_VECTOR_ITERATOR_CONVOP_REFCONV_AND_PTRSTY_##It ptrs;               \
        std::ptrdiff_t i;                                                      \
    };

  public:
    using value_type      = std::tuple<Ts...>;
    using reference       = std::tuple<Ts &...>;
    using const_reference = std::tuple<const Ts &...>;
    using allocator_type  = Al;
    SOA_VECTOR_ITERATOR(const_iterator, const_reference)
    SOA_VECTOR_ITERATOR(iterator, reference)

  private:
    struct End_iterator {
        operator iterator() const noexcept { return {.i = 0}; }
        operator const_iterator() const noexcept { return {.i = 0}; }
    };

  public:
    //
    // DEFAULT OPERATIONS
    //

    inline vector_impl() noexcept : al_() {}
    inline vector_impl(const Al &al) noexcept(noexcept(Al(al))) : al_(al) {}
    inline vector_impl(vector_impl &&other) noexcept
        : al_(std::move(other.al_)), p_(other.p_), sz_(other.sz_),
          cap_(other.cap_)
    {
        other.p_   = nullptr;
        other.sz_  = 0;
        other.cap_ = 0;
    }
    inline vector_impl(const vector_impl &other) : vector_impl(other.al_)
    {
        cap_ = other.cap_;
        sz_  = other.sz_;
        p_   = al_.allocate(cap_ * Sz_all);
        try {
            (std::uninitialized_copy_n(other.get<Is>(), sz_, get<Is>()), ...);
        } catch (...) {
            al_.deallocate(p_, cap_ * Sz_all);
            throw;
        }
    }

    inline vector_impl &operator=(const vector_impl &rhs)
    {
        vector_impl{rhs}.swap(*this);
        return *this;
    }

    inline vector_impl &operator=(vector_impl &&rhs)
    {
        vector_impl{std::move(rhs)}.swap(*this);
        return *this;
    }

    inline void swap(vector_impl &other)
    {
        std::swap(p_, other.p_);
        std::swap(sz_, other.sz_);
        std::swap(cap_, other.cap_);
    }

    inline ~vector_impl()
    {
        if (p_) {
            for (auto j = sz_; j--;)
                (std::destroy_at(&get<Is>()[j]), ...);
            al_.deallocate(p_, cap_ * Sz_all);
        }
    }

    //
    // MODIFIERS
    //

    iterator erase(const_iterator first, const_iterator last) noexcept(false)
    {
        auto f_idx           = sz_ + first.i;
        auto l_idx           = sz_;
        auto shift           = last.i - first.i;
        const auto operation = [&]<class T>(T *f, T *l) {
            // std::shift_left(f, l, shift);
            std::destroy(f + shift, l);
            return f;
        };
        return {std::tuple{operation(get<Is>() + f_idx, get<Is>() + l_idx)...},
                -static_cast<intptr_t>(sz_ -= shift)};
    }
    inline iterator erase(const_iterator pos)
    {
        return erase(pos, std::next(pos, 1));
    }

    inline void reserve(std::size_t cap)
    {
        if (cap <= cap_)
            return;
        if (!p_) {
            p_   = al_.allocate(cap * Sz_all);
            cap_ = cap;
        } else
            Re_alloc(cap);
    }

    inline std::enable_if_t<(std::is_default_constructible_v<Ts> && ...)>
    resize(std::size_t sz) noexcept(
        std::conjunction_v<std::is_nothrow_destructible<Ts>...>
            &&std::conjunction_v<std::is_nothrow_default_constructible<Ts>...>)
    {
        if (sz > sz_)
            Shrink_or_extend_at<false>(sz_, sz);
        else
            Shrink_or_extend_at<true>(sz, sz_);
        sz_ = sz;
    }

    template <class Tuple, std::enable_if_t<Is_tuple<Tuple>::value, int> = 0>
    inline void push_back(Tuple &&xs) noexcept(
        std::conjunction_v<std::is_nothrow_constructible<
            Ts, std::tuple_element_t<Is, Tuple>>...>)
    {
        push_back(std::get<Is>(std::forward<Tuple>(xs))...);
    }

    template <class... Us>
    inline std::enable_if_t<sizeof...(Us) == sizeof...(Ts)>
    push_back(Us &&...xs) noexcept(
        Emplace_or_push_back_is_noexcept<false, Us...>::value)
    {
        Emplace_or_push_back<false>(std::forward<Us>(xs)...);
    }

    template <class... Us>
    inline std::enable_if_t<sizeof...(Ts) == sizeof...(Us) &&
                            (Is_tuple<Us>::value && ...)>
    emplace_back(std::piecewise_construct_t, Us &&...xs) noexcept(
        Emplace_or_push_back_is_noexcept<true, Us...>::value)
    {
        Emplace_or_push_back<true>(std::forward<Us>(xs)...);
    }

    template <class... Us>
    inline std::enable_if_t<sizeof...(Ts) == sizeof...(Us)>
    emplace_back(Us &&...xs) noexcept(
        Emplace_or_push_back_is_noexcept<true, std::tuple<Us>...>::value)
    {
        emplace_back(std::piecewise_construct,
                     std::tuple<Us>{std::forward<Us>(xs)}...);
    }

    //
    // ACCESSORS
    //

    inline Al get_allocator() const noexcept { return al_; }

    template <std::size_t I>
    inline Elem<I> *get() noexcept
    {
        return Ith_arr<I>(p_, cap_);
    }

    template <std::size_t I>
    inline const Elem<I> *get() const noexcept
    {
        return Ith_arr<I>(p_, cap_);
    }

    inline reference operator[](std::size_t i) noexcept
    {
        assert(i < sz_);
        return At(i);
    }

    inline const_reference operator[](std::size_t i) const noexcept
    {
        assert(i < sz_);
        return At(i);
    }

    inline std::size_t size() const noexcept { return sz_; }
    inline std::size_t capacity() const noexcept { return cap_; }
    inline bool empty() const noexcept { return !sz_; }

    inline iterator begin() noexcept
    {
        return {{(get<Is>() + sz_)...}, -(intptr_t)sz_};
    }
    inline const_iterator begin() const noexcept
    {
        return {{(get<Is>() + sz_)...}, -(intptr_t)sz_};
    }

    inline const_iterator cbegin() const noexcept { return begin(); }

    inline End_iterator end() noexcept { return {}; }
    inline End_iterator cend() const noexcept { return {}; }
    inline End_iterator end() const noexcept { return {}; }

  private:
    template <bool Shrink>
    inline void Shrink_or_extend_at(std::size_t a, std::size_t b) noexcept(
        Shrink
            ? std::conjunction_v<std::is_nothrow_destructible<Ts>...>
            : std::conjunction_v<std::is_nothrow_default_constructible<Ts>...>)
    {
        const auto operation = []<class T>(T *first, T *last) {
            if constexpr (Shrink)
                std::destroy(first, last);
            else
                std::uninitialized_default_construct(first, last);
        };
        (operation(get<Is>() + a, get<Is>() + b), ...);
    }

    template <class, class Tuple,
              class = std::make_index_sequence<std::tuple_size_v<Tuple>>>
    struct Is_nothrow_emplaceable {
    };
    template <class T, class Tuple, std::size_t... Is>
    struct Is_nothrow_emplaceable<T, Tuple, std::index_sequence<Is...>>
        : std::is_nothrow_constructible<T, std::tuple_element_t<Is, Tuple>...> {
    };

    template <bool Emplace, class... Us>
    struct Emplace_or_push_back_is_noexcept
        : std::conjunction<Is_nothrow_emplaceable<Ts, std::decay_t<Us>>...> {
    };
    template <class... Us>
    struct Emplace_or_push_back_is_noexcept<false, Us...>
        : std::conjunction<std::is_nothrow_constructible<Ts, Us>...> {
    };

    template <bool Emplace, class... Us>
    inline void Emplace_or_push_back(Us &&...xs) noexcept(
        Emplace_or_push_back_is_noexcept<Emplace, Us...>::value)
    {
        DORI_explicit_new_and_delete_begin assert(sz_ < cap_);
        (..., [&]<class T, class U>(T &dst, U &&x) {
            auto addr = std::addressof(dst);
            assert((char *)addr >= p_);
            assert((char *)&addr[1] <= (char *)&p_[cap_ * Sz_all]);
            if constexpr (Emplace) {
                std::apply(
                    [&](auto &&...xs) {
                        new (addr) T(std::forward<decltype(xs)>(xs)...);
                    },
                    std::forward<U>(x));
            } else {
                new (addr) T(std::forward<U>(x));
            }
        }(get<Is>()[sz_], std::forward<Us>(xs)));
        ++sz_;
        DORI_explicit_new_and_delete_end
    }

    inline void Re_alloc(std::size_t cap)
    {
        assert(cap >= sz_);
        auto p             = al_.allocate(cap * Sz_all);
        const auto resizer = [&]<class T>(T *first, T *dest) {
            if constexpr (std::is_trivially_copy_constructible_v<T>)
                std::uninitialized_copy_n(first, sz_, dest);
            else
                std::uninitialized_move_n(first, sz_, dest);
            std::destroy_n(first, sz_);
        };
        (resizer(get<Is>(), Ith_arr<Is>(p, cap)), ...);
        al_.deallocate(std::exchange(p_, p), std::exchange(cap_, cap) * Sz_all);
    }

    inline reference At(std::size_t i) noexcept
    {
        return std::tie(Ith_arr<Is>(p_, cap_)[i]...);
    }

    inline const_reference At(std::size_t i) const noexcept
    {
        return std::tie(Ith_arr<Is>(p_, cap_)[i]...);
    }

    template <std::size_t I, class T, class Rev = std::false_type>
    static constexpr auto Ith_arr(T *p, std::size_t cap, Rev = {}) noexcept
    {
        constexpr std::size_t offset =
            Get_offsets()[I] + (Rev::value ? sizeof(Elem<I>) : 0);
        using ElemTy =
            std::conditional_t<std::is_const_v<T>, const Elem<I>, Elem<I>>;

        DORI_c_style_cast_begin return (ElemTy *)(&p[offset * cap]);
        DORI_c_style_cast_end
    }

    char *p_{};
    std::size_t sz_{}, cap_{};
    DORI_use_no_unique_address_begin [[no_unique_address]] Al al_;
    DORI_use_no_unique_address_end
};

template <class... Ts>
struct vector_caster {
    template <class Al, std::size_t... Is, class... Us>
    constexpr auto &
    operator()(vector_impl<Al, std::index_sequence<Is...>, Us...> &src) const
    {
        using Res = vector_impl<Al, std::index_sequence<Is...>, Ts...>;
        using Src = std::remove_reference_t<decltype(src)>;

        static_assert(sizeof...(Ts) == sizeof...(Us),
                      "vector dimensions must match");

        constexpr bool equal_type_sizes = [] {
            auto sz1 = std::array{sizeof(Ts)...};
            auto sz2 = std::array{sizeof(Us)...};
            std::sort(sz1.begin(), sz1.end(), std::greater<>{});
            std::sort(sz2.begin(), sz2.end(), std::greater<>{});
            return std::equal(sz1.begin(), sz1.end(), sz2.begin(), sz2.end());
        }();
        static_assert(equal_type_sizes,
                      "type sizes of given vectors must match");

        DORI_c_style_cast_begin return *(Res *)(&src);
        DORI_c_style_cast_end
    }
};

} // namespace detail

template <class... Ts>
constexpr detail::vector_caster<Ts...> vector_cast{};
template <class... Ts>
constexpr detail::vector_creator<Ts...> vector{};

} // namespace dori