#pragma once

#include <algorithm>
#include <array>
#include <assert.h>
#include <boost/align/aligned_allocator.hpp>
#include <memory>
#include <stdexcept>
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
        boost::alignment::aligned_allocator<char, std::max({alignof(Ts)...})>,
        std::index_sequence_for<Ts...>, Ts...>
    operator()() const noexcept
    {
        return {};
    }
};

template <class T>
using Move_t =
    std::conditional_t<std::is_trivially_copy_constructible_v<T>, T &, T &&>;

template <std::size_t I>
using Index = std::integral_constant<std::size_t, I>;

template <class Al, std::size_t... Is, class... Ts>
class vector_impl<Al, std::index_sequence<Is...>, Ts...>
{
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
    // Member functions
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
        p_   = Allocate(cap_ * Sz_all);
        try {
            (..., [&]<std::size_t I, class T>(Index<I>, const T *f, T *d_f) {
                for (const auto l = f + sz_; f != l; ++f, ++d_f)
                    Construct<I>(d_f, f[0]);
            }(Index<Is>{}, other.data<Is>(), data<Is>()));
        } catch (...) {
            Al_tr::deallocate(al_, p_, cap_ * Sz_all);
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

    inline ~vector_impl()
    {
        if (p_) {
            (..., [&]<class T>(T *f, T *l) {
                while (f != l)
                    Al_tr::destroy(al_, f++);
            }(data<Is>(), data<Is>() + sz_));
            Al_tr::deallocate(al_, p_, cap_ * Sz_all);
        }
    }

    inline Al get_allocator() const noexcept { return al_; }

    //
    // Element access
    //

    inline reference operator[](std::size_t i)
    {
        return {Ith_arr<Is>(p_, cap_)[i]...};
    }

    inline const_reference operator[](std::size_t i) const
    {
        return {Ith_arr<Is>(p_, cap_)[i]...};
    }

    inline reference at(std::size_t i)
    {
        if (i >= sz_)
            throw std::out_of_range{"dori::vector::at"};
        return operator[](i);
    }

    inline const_reference at(std::size_t i) const
    {
        if (i >= sz_)
            throw std::out_of_range{"dori::vector::at"};
        return operator[](i);
    }

    reference front() { return *begin(); }
    const_reference front() const { return *begin(); }

    reference back() { return *operator[](sz_ - 1); }
    const_reference back() const { return *operator[](sz_ - 1); }

    template <std::size_t I>
    inline Elem<I> *data() noexcept
    {
        return Ith_arr<I>(p_, cap_);
    }

    template <std::size_t I>
    inline const Elem<I> *data() const noexcept
    {
        return Ith_arr<I>(p_, cap_);
    }

    //
    // Iterators
    //

    inline iterator begin() noexcept
    {
        return {{(data<Is>() + sz_)...}, -(intptr_t)sz_};
    }
    inline const_iterator begin() const noexcept
    {
        return {{(data<Is>() + sz_)...}, -(intptr_t)sz_};
    }
    inline const_iterator cbegin() const noexcept { return begin(); }

    inline End_iterator cend() const noexcept { return {}; }
    inline End_iterator end() const noexcept { return {}; }

    //
    // Capacity
    //

    inline std::size_t size() const noexcept { return sz_; }
    inline std::size_t capacity() const noexcept { return cap_; }
    inline bool empty() const noexcept { return !sz_; }

    inline void reserve(std::size_t cap)
    {
        if (cap <= cap_)
            return;
        if (!p_) {
            p_   = Allocate(cap * Sz_all);
            cap_ = cap;
        } else
            Re_alloc(cap);
    }

    inline void resize(std::size_t sz) noexcept(
        std::conjunction_v<std::is_nothrow_destructible<Ts>...>
            &&std::conjunction_v<std::is_nothrow_default_constructible<
                Ts>...>) requires(std::is_default_constructible_v<Ts> &&...)
    {
        if (sz > sz_)
            Shrink_or_extend_at<false>(sz_, sz);
        else
            Shrink_or_extend_at<true>(sz, sz_);
        sz_ = sz;
    }

    inline void swap(vector_impl &other) noexcept
    {
        std::swap(p_, other.p_);
        std::swap(sz_, other.sz_);
        std::swap(cap_, other.cap_);
    }

    //
    // Modifiers
    //

    iterator erase(const_iterator first, const_iterator last)
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

    inline iterator erase(const_iterator pos)
    {
        return erase(pos, std::next(pos));
    }

    template <class Tpl>
    requires Is_tuple<Tpl>::value //
        inline void
        push_back(Tpl &&xs) noexcept((std::is_nothrow_constructible_v<
                                          Ts, std::tuple_element_t<Is, Tpl>> &&
                                      ...))
    {
        push_back(std::get<Is>(static_cast<Tpl &&>(xs))...);
    }

    template <class... Us>
    requires(sizeof...(Us) == sizeof...(Ts)) //
        inline void push_back(Us &&...xs) noexcept(
            noexcept(Emplace_or_push_back<false>(static_cast<Us &&>(xs)...)))
    {
        Emplace_or_push_back<false>(static_cast<Us &&>(xs)...);
    }

    template <class... Us>
    requires(sizeof...(Ts) == sizeof...(Us) && (Is_tuple<Us>::value && ...)) //
        inline void emplace_back(std::piecewise_construct_t, Us &&...xs) noexcept(
            noexcept(Emplace_or_push_back<true>(static_cast<Us &&>(xs)...)))
    {
        Emplace_or_push_back<true>(static_cast<Us &&>(xs)...);
    }

    template <class... Us>
    requires(sizeof...(Ts) == sizeof...(Us)) //
        inline void emplace_back(Us &&...xs) noexcept(
            Emplace_or_push_back_is_noexcept<true, std::tuple<Us>...>::value)
    {
        emplace_back(std::piecewise_construct,
                     std::tuple<Us>{static_cast<Us &&>(xs)}...);
    }

  private:
    //
    // Modification
    //

    template <bool Shrink>
    inline void Shrink_or_extend_at(std::size_t a, std::size_t b) noexcept(
        Shrink || (noexcept(Construct<Is>(std::declval<Ts *>())) && ...))
    {
        (..., [&]<std::size_t I, class T>(Index<I>, T *f, T *l) {
            while (f != l)
                if constexpr (Shrink)
                    Al_tr::destroy(al_, f++);
                else
                    Construct<I>(f++);
        }(Index<Is>{}, data<Is>() + a, data<Is>() + b));
    }

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
    inline void Emplace_or_push_back(Us &&...xs) noexcept(
        Emplace_or_push_back_is_noexcept<Emplace, Us...>::value)
    {
        DORI_explicit_new_and_delete_begin assert(sz_ < cap_);
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
        DORI_explicit_new_and_delete_end
    }

    //
    // Allocation
    //

    inline void Re_alloc(std::size_t cap)
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

    inline auto Allocate(std::size_t n)
    {
        assert(!(n % Sz_all));
        auto p = Al_tr::allocate(al_, n);
        assert(!(reinterpret_cast<uintptr_t>(p) % Align));
        return p;
    }

    template <std::size_t I, class... Args>
    inline void Construct(Elem<I> *p, Args &&...args) noexcept(
        noexcept(Al_tr::construct(al_, p, static_cast<Args &&>(args)...)))
    {
        try {
            Al_tr::construct(al_, p, static_cast<Args &&>(args)...);
        } catch (...) {
            Destruct_until<I>(p);
            throw;
        }
    }

    template <std::size_t I>
    inline void Destruct_until(Elem<I> *p) noexcept
    {
        const auto off = p - data<I>();
        (..., [&]<std::size_t J>(Index<J>) {
            auto f = data<J>();
            auto l = f + (J > I ? 0 : J < I ? sz_ : off);
            while (l != f)
                Al_tr::destroy(al_, --l);
        }(Index<sizeof...(Is) - 1 - Is>{}));
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

    char *p_         = nullptr;
    std::size_t sz_  = 0;
    std::size_t cap_ = 0;
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
            std::array sz1{sizeof(Ts)...};
            std::array sz2{sizeof(Us)...};
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