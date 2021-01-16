#pragma once

#include "detail/assert.h"
#include "detail/opaque_vector.h"
#include "detail/traits.h"
#include "detail/unsafe.h"
#include "detail/vector_caster.h"
#include "detail/vector_layout.h"

#include <tuple>

namespace dori
{

namespace detail
{

template <class Al, class... Ts, class... TsSrt, auto Offsets, auto Redir,
          std::size_t... Is>
class vector_impl<Al, Types<Ts...>, Types<TsSrt...>, Offsets, Redir, Is...>
    : opaque_vector<Al>
{
    using opaque_vector<Al>::al_;
    using opaque_vector<Al>::p_;
    using opaque_vector<Al>::sz_;
    using opaque_vector<Al>::cap_;
    using Al_tr = std::allocator_traits<Al>;

    static constexpr inline auto Sz_all = (sizeof(Ts) + ...);
    static constexpr inline auto Align  = std::max({alignof(Ts)...});

#define DORI_vector_iterator_convop_refconv_and_ptrsty_const_iterator          \
    std::tuple<const TsSrt *...>
#define DORI_vector_iterator_convop_refconv_and_ptrsty_iterator                \
    constexpr DORI_inline operator const_iterator() const noexcept             \
    {                                                                          \
        return {ptrs, i};                                                      \
    }                                                                          \
    std::tuple<TsSrt *...>

#define DORI_vector_iterator(It, Ref)                                          \
    struct It {                                                                \
        using difference_type   = vector_impl::difference_type;                \
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
            DORI_assert(i < 0 && "out-of-bounds access");                      \
            return {std::get<Redir[Is]>(ptrs)[i]...};                          \
        }                                                                      \
        constexpr DORI_inline bool operator==(const It &it) const noexcept     \
        {                                                                      \
            return i == it.i;                                                  \
        }                                                                      \
        constexpr DORI_inline bool operator!=(const It &it) const noexcept     \
        {                                                                      \
            return i != it.i;                                                  \
        }                                                                      \
        DORI_vector_iterator_convop_refconv_and_ptrsty_##It ptrs;              \
        std::ptrdiff_t i = 0;                                                  \
    }

  public:
    using value_type      = std::tuple<Ts...>;
    using reference       = std::tuple<Ts &...>;
    using const_reference = std::tuple<const Ts &...>;
    using difference_type = std::ptrdiff_t;
    using size_type       = std::size_t;
    using allocator_type  = Al;

    DORI_vector_iterator(const_iterator, const_reference);
    DORI_vector_iterator(iterator, reference);

  public:
    constexpr DORI_inline vector_impl() noexcept(noexcept(Al{}))
        : opaque_vector<Al>{}
    {
    }
    constexpr DORI_inline vector_impl(const Al &alloc) noexcept
        : opaque_vector<Al>{alloc}
    {
    }
    constexpr DORI_inline vector_impl(vector_impl &&other) noexcept
        : opaque_vector<Al>{static_cast<Al &&>(other.al_), other.p_, other.sz_,
                            other.cap_}
    {
        other.sz_  = 0;
        other.cap_ = 0;
    }

  private:
    constexpr DORI_inline auto
    Allocate(size_type n) noexcept(noexcept(Al_tr::allocate(al_, n)))
    {
        DORI_assert(n % Sz_all == 0);
        // Use of lambda here avoids unreachable code warning
        return [](auto p) {
            DORI_assert(reinterpret_cast<uintptr_t>(p) % Align == 0);
            return p;
        }(Al_tr::allocate(al_, n));
    }

  public:
    constexpr DORI_inline vector_impl(vector_impl &&other, const Al &alloc)
        : opaque_vector<Al>{alloc, other.p_, other.sz_, other.cap_}
    {
        if (!Al_tr::is_always_equal::value && alloc != other.al_) {
            auto p = Allocate(cap_ * Sz_all);
            Move_to_alloc(cap_, p);
            p_ = p;
        }
        other.sz_  = 0;
        other.cap_ = 0;
    }

  private:
    constexpr DORI_inline void Destroy_to(void *p, size_type f = 0) noexcept
    {
        Destroy_to_impl<Offsets[Is]...>::template fn<Al, TsSrt...>(*this, p, f);
    }

    constexpr DORI_inline void Copy_from(const vector_impl &v)
    {
        DORI_assert(!cap_ && !sz_);
        //
        // If an exception is thrown, p_ points to garbage. Due to this, use
        // !cap_ to check for no allocation.
        //
        sz_ = cap_ = v.sz_;
        if (v.cap_) {
            p_ = Allocate(cap_ * Sz_all);
            (..., [&]<size_type I, class T>(const T *f, T *d_f) {
                try {
                    for (const auto l = f + v.sz_; f != l; ++f, ++d_f)
                        Al_tr::construct(al_, d_f, *f);
                } catch (...) {
                    Destroy_to(d_f);
                    sz_ = cap_ = 0;
                    Al_tr::deallocate(al_, p_, v.cap_ * Sz_all);
                    throw;
                }
            }.template operator()<Is>(v.data<Is>(), data<Is>()));
        } else
            p_ = nullptr;
    }

  public:
    constexpr DORI_inline vector_impl(const vector_impl &other)
        : opaque_vector<Al>{
              Al_tr::select_on_container_copy_construction(other.al_)}
    {
        Copy_from(other);
    }
    constexpr DORI_inline vector_impl(const vector_impl &other, const Al &alloc)
        : opaque_vector<Al>{alloc}
    {
        Copy_from(other);
    }

  private:
    using Al_pocca = typename Al_tr::propagate_on_container_copy_assignment;
    using Al_pocma = typename Al_tr::propagate_on_container_move_assignment;
    using Al_pocs  = typename Al_tr::propagate_on_container_swap;
    using Al_iae   = typename Al_tr::is_always_equal;

    template <bool Move, class Vector>
    constexpr DORI_inline void Assign_from(Vector &v) noexcept(Move)
    {
        const auto mid = std::min(sz_, v.sz_);
        (..., [&]<class T>(auto f, T *d_f) {
            using Fwd_t = std::conditional_t<Move, T &&, const T &>;
            const T *a = d_f + mid, *b = d_f + v.sz_, *c = d_f + sz_;
            // Move into 0..mid
            for (; d_f != a; ++f, ++d_f)
                Call_maybe_unsafe(
                    [](T *x, auto y) { *x = static_cast<Fwd_t>(*y); }, d_f, f);
            // Umove into mid..rhs.sz_
            for (; d_f != b; ++f, ++d_f)
                Call_maybe_unsafe(DORI_f_ref(Al_tr::construct), al_, d_f,
                                  static_cast<Fwd_t>(*f));
            // Destroy rhs.sz_..sz_
            for (; std::less<>{}(d_f, c); ++f)
                Call_maybe_unsafe(DORI_f_ref(Al_tr::destroy), al_, d_f);
        }(v.Get_data<Is>(v.cap_), Get_data<Is>(cap_)));
    }

    constexpr DORI_inline void Maybe_delete() noexcept(
        noexcept(clear(), Al_tr::deallocate(al_, p_, cap_ *Sz_all)))
    {
        if (cap_) {
            clear();
            Al_tr::deallocate(al_, p_, cap_ * Sz_all);
            // Note no resetting vars
        }
    }

  public:
    constexpr DORI_inline vector_impl &operator=(const vector_impl &rhs)
    { // Note: The standard doesn't mandate strong exception guarantee.
        const bool keep_al =
            !Al_pocca::value || Al_iae::value || rhs.al_ == al_;
        if (!keep_al || cap_ < rhs.sz_) {
            Maybe_delete();
            sz_ = cap_ = 0;
            if (!keep_al) {
                al_ = rhs.al_;
                if (!sz_)
                    return *this;
            }
            p_   = Allocate(rhs.sz_ * Sz_all);
            cap_ = rhs.sz_;
        }
        Assign_from<false>(rhs);
        return *this;
    }

    constexpr DORI_inline vector_impl &
    operator=(vector_impl &&rhs) noexcept(Al_pocma::value || Al_iae::value)
    { // Note: The standard doesn't mandate strong exception guarantee.
        Maybe_delete();
        if constexpr (Al_iae::value) {
        } else if constexpr (Al_pocma::value) {
            al_ = static_cast<Al &&>(rhs.al_);
        } else if (al_ != rhs.al_) {
            if (cap_ < rhs.sz_) {
                cap_ = sz_ = 0;
                p_         = Allocate(rhs.sz_ * Sz_all);
                cap_       = rhs.sz_;
            }
            Assign_from<true>(rhs);
            return *this;
        }
        p_      = rhs.p_;
        sz_     = rhs.sz_;
        cap_    = rhs.cap_;
        rhs.sz_ = rhs.cap_ = 0;
        return *this;
    }

    constexpr DORI_inline void
    swap(vector_impl &other) noexcept(Al_pocs::value || Al_iae::value)
    {
        if constexpr (Al_iae::value) {
        } else if constexpr (Al_pocs::value) {
            using std::swap;
            swap(al_, other.al_);
        } else
            DORI_assert(al_ == other.al_);
        std::swap(p_, other.p_);
        std::swap(sz_, other.sz_);
        std::swap(cap_, other.cap_);
    }

    constexpr DORI_inline ~vector_impl() { Maybe_delete(); }

    constexpr DORI_inline Al get_allocator() const noexcept { return al_; }

    constexpr DORI_inline reference operator[](size_type i) noexcept
    {
        DORI_assert(i < sz_);
        return {data<Is>()[i]...};
    }

    constexpr DORI_inline const_reference operator[](size_type i) const noexcept
    {
        DORI_assert(i < sz_);
        return {data<Is>()[i]...};
    }

    constexpr DORI_inline reference at(size_type i)
    {
        if (i >= sz_)
            throw std::out_of_range{"dori::vector::at"};
        return operator[](i);
    }

    constexpr DORI_inline const_reference at(size_type i) const
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
    constexpr DORI_inline const_reference front() const noexcept
    {
        DORI_assert(sz_ > 0);
        return *begin();
    }

    constexpr DORI_inline reference back() noexcept
    {
        DORI_assert(sz_ > 0);
        return *operator[](sz_ - 1);
    }
    constexpr DORI_inline const_reference back() const noexcept
    {
        DORI_assert(sz_ > 0);
        return *operator[](sz_ - 1);
    }

  private:
    static constexpr auto npos = static_cast<size_type>(-1);
    template <std::size_t I>
    requires(I < sizeof...(Ts)) constexpr DORI_inline
        auto Get_data(size_type cap = npos) noexcept
    {
        const auto n = (cap == npos) ? cap_ : cap;
        using RTy    = typename Types<TsSrt...>::template Ith_t<I> *;
        return reinterpret_cast<RTy>(p_ + Offsets[I] * n);
    }

    template <std::size_t I>
    requires(I < sizeof...(Ts)) constexpr DORI_inline
        auto Get_data(size_type cap = npos) const noexcept
    {
        const auto n = (cap == npos) ? cap_ : cap;
        using RTy    = const typename Types<TsSrt...>::template Ith_t<I> *;
        return reinterpret_cast<RTy>(p_ + Offsets[I] * n);
    }

  public:
    template <std::size_t I>
    requires(I < sizeof...(Ts)) constexpr DORI_inline auto data() noexcept
    {
        DORI_assert(cap_);
        return Get_data<Redir[I]>(cap_);
    }

    template <std::size_t I>
    requires(I < sizeof...(Ts)) constexpr DORI_inline auto data() const noexcept
    {
        DORI_assert(cap_);
        return Get_data<Redir[I]>(cap_);
    }

  private:
    constexpr DORI_inline auto Iter_at(size_type idx) noexcept
    {
        return iterator{{(Get_data<Is>() + sz_)...},
                        static_cast<std::ptrdiff_t>(idx - sz_)};
    }
    constexpr DORI_inline auto Iter_at(size_type idx) const noexcept
    {
        return const_iterator{{(Get_data<Is>() + sz_)...},
                              static_cast<std::ptrdiff_t>(idx - sz_)};
    }

  public:
    constexpr DORI_inline auto begin() noexcept { return Iter_at(0); }
    constexpr DORI_inline auto begin() const noexcept { return Iter_at(0); }
    constexpr DORI_inline auto cbegin() const noexcept { return Iter_at(0); }

    constexpr DORI_inline iterator end() noexcept { return {}; }
    constexpr DORI_inline const_iterator end() const noexcept { return {}; }
    constexpr DORI_inline const_iterator cend() const noexcept { return {}; }

    constexpr DORI_inline bool empty() const noexcept { return !sz_; }
    constexpr DORI_inline size_type size() const noexcept { return sz_; }
    constexpr DORI_inline size_type capacity() const noexcept { return cap_; }

  private:
    constexpr DORI_inline void Move_to_alloc(size_type cap, auto p) noexcept
    {
        DORI_assert(cap >= sz_);
        (..., [&]<class T>(T *f, T *d_f) {
            for (const auto l = f + sz_; f != l; ++f, ++d_f) {
                Call_maybe_unsafe(DORI_f_ref(Al_tr::construct), al_, d_f,
                                  static_cast<Move_t<T>>(*f));
                Call_maybe_unsafe(DORI_f_ref(Al_tr::destroy), al_, f);
            }
        }(data<Is>(), reinterpret_cast<Ts *>(p + Offsets[Is] * cap)));
    }

  public:
    constexpr DORI_inline void
    reserve(size_type cap) noexcept(noexcept(Move_to_alloc(cap, Allocate({}))))
    {
        DORI_assert(cap > cap_);
        auto p = Allocate(cap * Sz_all);
        if (cap_) {
            Move_to_alloc(cap, p);
            Al_tr::deallocate(al_, p_, cap_ * Sz_all);
        }
        p_   = p;
        cap_ = cap;
    }

    constexpr DORI_inline void
    shrink_to_fit() noexcept(noexcept(Move_to_alloc(sz_, Allocate({}))))
    {
        DORI_assert(sz_); // use '= {}' to empty
        auto p = Allocate(sz_ * Sz_all);
        Move_to_alloc(sz_, p);
        Al_tr::deallocate(al_, p_, cap_ * Sz_all);
        p_   = p;
        cap_ = sz_;
    }

    constexpr DORI_inline void clear() noexcept
    {
        (..., [&]<class T>(T *f, T *l) {
            while (f != l)
                Call_maybe_unsafe(DORI_f_ref(Al_tr::destroy), al_, f++);
        }(Get_data<Is>(), Get_data<Is>() + sz_));
        sz_ = 0;
    }

    constexpr DORI_inline iterator
    erase(const_iterator first, const_iterator last) noexcept(
        (noexcept(std::declval<Ts &>() = std::declval<Ts &&>()) && ...))
    {
        const auto f_i = sz_ + first.i;
        const auto n   = last.i - first.i;
        const std::tuple ptrs{(&data<Is>()[sz_] + f_i)...};
        (..., [&]<class T>(T *d_f) {
            const auto e = d_f - first.i;
            for (auto f = d_f + n; f != e; ++f, ++d_f)
                *d_f = static_cast<T &&>(*f);
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

    template <Tuple... Us>
    requires(sizeof...(Ts) == sizeof...(Us)) //
        constexpr DORI_inline iterator
        emplace_back(std::piecewise_construct_t, Us &&...xs) noexcept(
            (... && std::is_same_v<Us, std::tuple<Ts &&>>))
    {
        DORI_assert(sz_ < cap_);
        std::tuple<Us &&...> fwd{static_cast<Us &&>(xs)...};
        const auto off = sz_++;
        (..., ([&]<class T, class U, std::size_t... Js>(
                  T * p, U && t, std::index_sequence<Js...>) {
             using D = std::decay_t<U>;
             static_assert(
                 std::is_constructible_v<T, std::tuple_element_t<Js, D> &&...>,
                 "elements not constructible with parameters to emplace()");
             try {
                 Call_maybe_unsafe(std::is_same<std::tuple<T &&>, D>{},
                                   DORI_f_ref(Al_tr::construct), al_, p,
                                   std::get<Js>(static_cast<U &&>(t))...);
             } catch (...) {
                 Destroy_to(p, off);
                 --sz_;
                 throw;
             }
         }(Get_data<Is>() + off,
              std::get<Redir[Is]>(static_cast<std::tuple<Us &&...> &&>(fwd)),
              std::make_index_sequence<
                  std::tuple_size_v<Ith_t<Redir[Is], Us...>>>{})));
        return Iter_at(off);
    }

    constexpr DORI_inline auto emplace_back() noexcept(noexcept(
        emplace_back(std::piecewise_construct, (Is, std::tuple<>{})...))) //
        requires(... &&std::is_default_constructible_v<Ts>)
    {
        return emplace_back(std::piecewise_construct, (Is, std::tuple<>{})...);
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

    constexpr DORI_inline void resize(size_type sz)
    {
        DORI_assert(cap_);
        if (sz > sz_) { // proposed exceeds current => extend
            const auto off = sz_;
            sz_            = sz;
            (..., [&]<class T>(T *f, T *l) {
                try {
                    for (; f != l; ++f)
                        Al_tr::construct(al_, f);
                } catch (...) {
                    Destroy_to(f, off);
                    sz_ = off;
                    throw;
                }
            }(data<Is>() + off, data<Is>() + sz));
        } else { // current exceeds proposed => shrink
            (..., [&]<class T>(T *f, T *l) {
                while (f != l)
                    Call_maybe_unsafe(DORI_f_ref(Al_tr::destroy), al_, f++);
            }(data<Is>() + sz, data<Is>() + sz_));
            sz_ = sz;
        }
    }

    template <class F>
    requires((std::is_invocable_v<F &&, Ts *, Ts *> && ...)) //
        constexpr DORI_inline void for_each(F &&f) noexcept(
            noexcept((..., static_cast<F &&>(f)(data<Is>(), data<Is>()))))
    {
        (...,
         static_cast<F &&>(f)(data<Redir[Is]>() + data<Redir[Is]>() + sz_));
    }
    template <class F>
    requires((std::is_invocable_v<F &&, Ts *, Ts *> && ...)) //
        constexpr DORI_inline void for_each(F &&f) const
        noexcept(noexcept((..., static_cast<F &&>(f)(data<Is>(), data<Is>()))))
    {
        (...,
         static_cast<F &&>(f)(data<Redir[Is]>() + data<Redir[Is]>() + sz_));
    }
    template <class F>
    requires((std::is_invocable_v<F &&, Ts *, Ts *> && ...)) //
        constexpr DORI_inline void for_each_stable(F &&f) noexcept(
            noexcept((..., static_cast<F &&>(f)(data<Is>(), data<Is>()))))
    {
        (..., static_cast<F &&>(f)(data<Is>() + data<Is>() + sz_));
    }
    template <class F>
    requires((std::is_invocable_v<F &&, Ts *, Ts *> && ...)) //
        constexpr DORI_inline void for_each_stable(F &&f) const
        noexcept(noexcept((..., static_cast<F &&>(f)(data<Is>(), data<Is>()))))
    {
        (..., static_cast<F &&>(f)(data<Is>() + data<Is>() + sz_));
    }
};

} // namespace detail

template <class... Ts>
constexpr inline detail::vector_caster<Ts...> vector_cast{};

template <class Allocator, class... Ts>
struct vector_al : detail::Get_vector_t<Allocator, Ts...> {
    using detail::Get_vector_t<Allocator, Ts...>::vector_impl;
};

template <class Al, class... Ts>
constexpr DORI_inline bool operator==(const vector_al<Al, Ts...> &lhs,
                                      const vector_al<Al, Ts...> &rhs) noexcept
{
    return [&]<std::size_t... Is>(std::index_sequence<Is...>)
    {
        return (... && std::equal(lhs.data<Is>(), lhs.data<Is>() + lhs.size(),
                                  rhs.data<Is>(), rhs.data<Is>() + rhs.size()));
    }
    (std::index_sequence_for<Ts...>{});
}

template <class Al, class... Ts>
constexpr DORI_inline bool operator!=(const vector_al<Al, Ts...> &lhs,
                                      const vector_al<Al, Ts...> &rhs) noexcept
{
    return !(lhs == rhs);
}

template <class Al, class... Ts>
constexpr DORI_inline void swap(vector_al<Al, Ts...> &lhs,
                                vector_al<Al, Ts...> &rhs) noexcept
{
    lhs.swap(rhs);
}

template <class... Ts>
using vector = vector_al<
    boost::alignment::aligned_allocator<char, std::max({alignof(Ts)...})>,
    Ts...>;

} // namespace dori