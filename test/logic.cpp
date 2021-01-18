#include <concepts>
#include <dori/all.h>
#include <numeric>
#include <stdint.h>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

using namespace std;

//
// Assert conformance with AllocatorAwareContainer
// https://en.cppreference.com/w/cpp/named_req/AllocatorAwareContainer
//

using C = dori::vector<int>;
static_assert(regular<C>);
static_assert(swappable<C>);
static_assert(destructible<C>);
// static_assert(same_as<C::allocator_type::value_type, C::value_type>);
// static_assert(same_as<C::reference, C::value_type &>);
static_assert(convertible_to<C::iterator, C::const_iterator>);
static_assert(signed_integral<C::difference_type>);
static_assert(
    same_as<iterator_traits<C::iterator>::difference_type, C::difference_type>);
static_assert(same_as<iterator_traits<C::const_iterator>::difference_type,
                      C::difference_type>);
static_assert(unsigned_integral<C::size_type>);

//
// Check member functions
//
// __VA_OPT__
#define LOGIC_check_memfn(r, f, v, ...)                                        \
    static_assert(is_invocable_r_v<r, decltype(DORI_f_ref(declval<v>().f)),    \
                                   ##__VA_ARGS__>)
#define LOGIC_check_nonmemfn(r, f, ...)                                        \
    static_assert(is_invocable_r_v<r, decltype(DORI_f_ref(f)), ##__VA_ARGS__>)
LOGIC_check_memfn(C::allocator_type, get_allocator, const C &);
LOGIC_check_memfn(C::iterator, begin, C &);
LOGIC_check_memfn(C::iterator, end, C &);
LOGIC_check_memfn(C::const_iterator, cbegin, C &);
LOGIC_check_memfn(C::const_iterator, cend, C &);
LOGIC_check_memfn(C::const_iterator, begin, const C &);
LOGIC_check_memfn(C::const_iterator, end, const C &);
LOGIC_check_memfn(C::const_iterator, cbegin, const C &);
LOGIC_check_memfn(C::const_iterator, cend, const C &);
LOGIC_check_memfn(C::size_type, size, const C &);
// LOGIC_check_memfn(C::size_type, max_size, C &);
LOGIC_check_memfn(bool, empty, C &);

template <class T>
struct Swappable {
    static int swaps;
};
template <class T>
int Swappable<T>::swaps = 0;

template <class T>
requires std::is_base_of_v<Swappable<T>, T> void swap(T &, T &)
{
    ++Swappable<T>::swaps;
}

TEST_SUITE("dori::vector")
{
#define DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(X)                           \
    static int X##_def_ctors   = 0;                                            \
    static int X##_move_ctors  = 0;                                            \
    static int X##_copy_ctors  = 0;                                            \
    static int X##_dtors       = 0;                                            \
    static int X##_copy_asgmts = 0;                                            \
    static int X##_move_asgmts = 0;                                            \
    struct X {                                                                 \
        X() noexcept { ++X##_def_ctors; }                                      \
        X &operator=(const X &) noexcept { return ++X##_copy_asgmts, *this; }  \
        X &operator=(X &&) noexcept { return ++X##_move_asgmts, *this; }       \
        X(X &&) noexcept { ++X##_move_ctors; }                                 \
        X(const X &) noexcept { ++X##_copy_ctors; }                            \
        ~X() noexcept { ++X##_dtors; }                                         \
    };

    TEST_CASE("resize manages construction and destruction of objects")
    {
        DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(A)
        DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(B)
        dori::vector<A, B> v;

        v.reserve(8);
        REQUIRE_EQ(A_def_ctors, 0);
        REQUIRE_EQ(B_def_ctors, 0);
        REQUIRE_EQ(A_dtors, 0);
        REQUIRE_EQ(B_dtors, 0);

        v.resize(8);
        REQUIRE_EQ(A_def_ctors, 8);
        REQUIRE_EQ(B_def_ctors, 8);
        REQUIRE_EQ(A_dtors, 0);
        REQUIRE_EQ(B_dtors, 0);

        v.resize(0);
        REQUIRE_EQ(A_def_ctors, 8);
        REQUIRE_EQ(B_def_ctors, 8);
        REQUIRE_EQ(A_dtors, 8);
        REQUIRE_EQ(B_dtors, 8);
    }

    TEST_CASE("destructor of dori::vector destructs contained objects")
    {
        DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(A)
        DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(B)
        {
            dori::vector<A, B> v;
            v.reserve(8);
            v.resize(8);
        }
        REQUIRE_EQ(A_def_ctors, 8);
        REQUIRE_EQ(B_def_ctors, 8);
        REQUIRE_EQ(A_dtors, 8);
        REQUIRE_EQ(B_dtors, 8);
    }

    TEST_CASE("reserve moves objects over to the new range")
    {
        DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(A)
        DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(B)
        dori::vector<A, B> v;

        v.reserve(8);
        v.resize(8);
        REQUIRE_LT(v.capacity(), 16);
        REQUIRE_EQ(v.size(), 8);

        v.reserve(16);
        REQUIRE(v.capacity() >= 16);
        REQUIRE_EQ(A_move_ctors, 8);
        REQUIRE_EQ(B_move_ctors, 8);
        REQUIRE_EQ(A_dtors, 8);
        REQUIRE_EQ(B_dtors, 8);
    }

    TEST_CASE("")
    {
        dori::vector<int8_t, int64_t, int16_t, int32_t> v;
        v.reserve(1);
        v.resize(1);
        auto [i8, i64, i16, i32] = v[0];
        SUBCASE("order of element sequences respects elements' alignment")
        {
            /*auto lo = dori::detail::Offsets<int8_t, int64_t, int16_t,
            int32_t>; (void)lo;*/
            REQUIRE_LT((uintptr_t)&i64, (uintptr_t)&i32);
            REQUIRE_LT((uintptr_t)&i32, (uintptr_t)&i16);
            REQUIRE_LT((uintptr_t)&i16, (uintptr_t)&i8);
        }
        SUBCASE("contained elements are aligned properly")
        {
            REQUIRE_EQ((uintptr_t)&i64 % alignof(int64_t), 0);
            REQUIRE_EQ((uintptr_t)&i32 % alignof(int32_t), 0);
            REQUIRE_EQ((uintptr_t)&i16 % alignof(int16_t), 0);
            REQUIRE_EQ((uintptr_t)&i8 % alignof(int8_t), 0);
        }
    }

    TEST_CASE("dori::vector::erase erases expectedly")
    {
        static size_t nctors = 0;
        static size_t nmoves = 0;
        static size_t ndtors = 0;
        struct S {
            S() { ++nctors; }
            S(S &&) { ++nmoves; }
            S &operator=(S &&) { return ++nmoves, *this; }
            ~S() { ++ndtors; }
        };
        dori::vector<S> v;
        REQUIRE_EQ(v.size(), 0);
        REQUIRE_EQ(nctors, 0);
        REQUIRE_EQ(nmoves, 0);
        REQUIRE_EQ(ndtors, 0);
        v.reserve(8);
        REQUIRE_EQ(v.size(), 0);
        REQUIRE_EQ(nctors, 0);
        REQUIRE_EQ(nmoves, 0);
        REQUIRE_EQ(ndtors, 0);
        v.resize(8);
        REQUIRE_EQ(v.size(), 8);
        REQUIRE_EQ(nctors, 8);
        REQUIRE_EQ(nmoves, 0);
        REQUIRE_EQ(ndtors, 0);
        v.erase(v.begin(), next(v.begin(), 4));
        REQUIRE_EQ(v.size(), 4);
        REQUIRE_EQ(nctors, 8);
        REQUIRE_EQ(nmoves, 4);
        REQUIRE_EQ(ndtors, 4);
        v.erase(v.begin(), v.end());
        REQUIRE_EQ(v.size(), 0);
        REQUIRE_EQ(nctors, 8);
        REQUIRE_EQ(nmoves, 4);
        REQUIRE_EQ(ndtors, 8);
    }

    TEST_CASE("dori::vector works with algorithms")
    {
        dori::vector<int> v;
        using It        = decltype(v)::iterator;
        const auto eq   = [](auto a, auto b) { return get<0>(a) == get<0>(b); };
        const auto even = [](auto x) { return get<0>(x) % 2 == 0; };

        v.reserve(8);
        tuple<int> nums[]{{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}};
        copy(&nums[0], &nums[8], back_inserter(v));
        REQUIRE(equal<It>(v.begin(), v.end(), &nums[0], eq));

        v.erase(remove_if<It>(v.begin(), v.end(), even), v.end());
        tuple<int> odds[]{{1}, {3}, {5}, {7}};
        REQUIRE_EQ(v.size(), size(odds));
        REQUIRE(equal<It>(v.begin(), v.end(), odds, eq));

        decltype(v) v2;
        v2.reserve(v.size());
        transform<It>(v.begin(), v.end(), odds, back_inserter(v2),
                      [](auto a, auto b) -> tuple<int> {
                          return {get<0>(a) + get<0>(b)};
                      });
        tuple<int> odds_doubled[]{{2}, {6}, {10}, {14}};
        REQUIRE_EQ(v2.size(), size(odds_doubled));
        REQUIRE(equal<It>(v2.begin(), v2.end(), odds_doubled, eq));

        const int sum =
            reduce<It>(++v2.begin(), v2.end(), get<0>(v2[0]),
                       [](int acc, auto cur) { return acc + get<0>(cur); });
        REQUIRE_EQ(sum, 2 + 6 + 10 + 14);
    }

    TEST_CASE("")
    {
        DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(A)
        DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(B)
        dori::vector<A, B> v;
        v.reserve(8);
        v.resize(8);

        SUBCASE("dori::vector can be copy-constructed")
        {
            auto v2 = v;
            REQUIRE_EQ(v2.size(), v.size());
            REQUIRE_EQ(A_def_ctors, 8);
            REQUIRE_EQ(A_copy_ctors, 8);
            REQUIRE_EQ(A_move_ctors, 0);

            SUBCASE("dori::vector can be move-constructed")
            {
                auto v3 = move(v);
                REQUIRE_EQ(v.size(), 0);
                REQUIRE_EQ(v3.size(), v2.size());
                REQUIRE_EQ(A_def_ctors, 8);
                REQUIRE_EQ(A_copy_ctors, 8);
                REQUIRE_EQ(A_move_ctors, 0);
            }
        }

        REQUIRE_EQ(A_dtors, 16);
        REQUIRE_EQ(B_dtors, 16);
    }

    TEST_CASE("dori::vector::emplace_back works")
    {
        SUBCASE("dori::vector::emplace_back initializes expectedly")
        {
            dori::vector<int, int> v;
            v.reserve(5);
            for (int i = 0; i < 5; ++i)
                v.emplace_back(i, i * 2);
            for (int i = 0; i < 5; ++i) {
                REQUIRE_EQ(v.data<0>()[i], i);
                REQUIRE_EQ(v.data<1>()[i], i * 2);
            }
        }
        SUBCASE("dori::vector::emplace_back forwards expectedly")
        {
            SUBCASE("an rvalue reference")
            {
                DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(A)
                {
                    dori::vector<A> v;
                    v.reserve(1);
                    v.emplace_back(A{});
                }
                REQUIRE_EQ(A_def_ctors, 1);
                REQUIRE_EQ(A_move_ctors, 1);
                REQUIRE_EQ(A_copy_ctors, 0);
                REQUIRE_EQ(A_dtors, 2);
            }
            SUBCASE("an lvalue reference")
            {
                DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(A)
                {
                    A a;
                    dori::vector<A> v;
                    v.reserve(1);
                    v.emplace_back(a);
                }
                REQUIRE_EQ(A_def_ctors, 1);
                REQUIRE_EQ(A_move_ctors, 0);
                REQUIRE_EQ(A_copy_ctors, 1);
                REQUIRE_EQ(A_dtors, 2);
            }
        }
    }

    TEST_CASE("dori::vector::push_back works")
    {
        dori::vector<int, int> v;
        v.reserve(5);
        for (int i = 0; i < 5; ++i)
            v.push_back({i, i * 2});
        for (int i = 0; i < 5; ++i) {
            REQUIRE_EQ(v.data<0>()[i], i);
            REQUIRE_EQ(v.data<1>()[i], i * 2);
        }
    }

    TEST_CASE("dori::vector_cast works")
    {
        SUBCASE("result of dori::vector_cast uses its own offset array")
        {
            dori::vector<int, double> v;
            auto &v2 = dori::vector_cast<double, unsigned>(v);
            v.reserve(8);
            for (int x : {0, 1, 2, 3, 4, 5, 6, 7})
                v.push_back(x, x * 2.);

            auto [v_i0, v_d0]   = v[0];
            auto [v2_d0, v2_u0] = v2[0];
            REQUIRE_EQ(v_i0, 0);
            REQUIRE_EQ(v2_d0, 0);
            REQUIRE_EQ(v_i0, v2_u0);
            REQUIRE_EQ(v_d0, v2_d0);

            auto [v_i7, v_d7]   = v[7];
            auto [v2_d7, v2_u7] = v2[7];
            REQUIRE_EQ(v_i7, 7);
            REQUIRE_EQ(v2_d7, 14.);
            REQUIRE_EQ(v_i7, v2_u7);
            REQUIRE_EQ(v_d7, v2_d7);
        }
        SUBCASE("dori::vector can be copy-constructed from "
                "dori::vector_cast")
        {
            DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(A)
            DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(B)
            {
                dori::vector<A> v;
                v.reserve(8);
                v.resize(8);
                auto v2 = dori::vector_cast<B>(v);
            }
            REQUIRE_EQ(A_def_ctors, 8);
            REQUIRE_EQ(B_copy_ctors, 8);
            REQUIRE_EQ(A_dtors, 8);
            REQUIRE_EQ(B_dtors, 8);
        }
    }

#define DORI_VECTOR_TEST_EXPLICIT_NEW_AND_DELETE_BEGIN                         \
    __pragma(warning(push)) __pragma(warning(disable : 26409))
#define DORI_VECTOR_TEST_EXPLICIT_NEW_AND_DELETE_END __pragma(warning(pop))

    TEST_CASE("dori::vector supports custom allocators")
    {
        SUBCASE("dori::vector interfaces correctly with allocator")
        {
            DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(A)
            static size_t allocations = 0;
            struct Al : A {
                using A::A;
                using is_always_equal = true_type;
                using value_type      = std::byte;
                bool operator==(const Al &) const { return true; }
                bool operator!=(const Al &) const { return false; }
                std::byte *allocate(size_t n)
                {
                    allocations += n;
                    DORI_VECTOR_TEST_EXPLICIT_NEW_AND_DELETE_BEGIN
                    return new std::byte[n];
                    DORI_VECTOR_TEST_EXPLICIT_NEW_AND_DELETE_END
                }
                void deallocate(void *p, size_t n) noexcept
                {
                    allocations -= n;
                    DORI_VECTOR_TEST_EXPLICIT_NEW_AND_DELETE_BEGIN
                    delete[](char *) p;
                    DORI_VECTOR_TEST_EXPLICIT_NEW_AND_DELETE_END
                }
            };

            {
                dori::vector<int, Al> v;
                REQUIRE_EQ(A_def_ctors, 1);
                REQUIRE_EQ(A_copy_ctors, 0);
                REQUIRE_EQ(A_move_ctors, 0);
                REQUIRE_EQ(A_dtors, 0);

                v.reserve(2);
                REQUIRE_EQ(allocations, sizeof(int) * 2);
                REQUIRE_EQ(A_def_ctors, 1);
                REQUIRE_EQ(A_copy_ctors, 0);
                REQUIRE_EQ(A_move_ctors, 0);
                REQUIRE_EQ(A_dtors, 0);

                v.resize(2);
                REQUIRE_EQ(allocations, sizeof(int) * 2);
                REQUIRE_EQ(A_def_ctors, 1);
                REQUIRE_EQ(A_copy_ctors, 0);
                REQUIRE_EQ(A_move_ctors, 0);
                REQUIRE_EQ(A_dtors, 0);

                auto v2{v};
                REQUIRE_EQ(allocations, sizeof(int) * 2 * 2);
                REQUIRE_EQ(A_def_ctors, 1);
                REQUIRE_EQ(A_copy_ctors, 1);
                REQUIRE_EQ(A_move_ctors, 0);
                REQUIRE_EQ(A_dtors, 0);
                REQUIRE_EQ(v.size(), 2);
                REQUIRE_EQ(v.capacity(), 2);
                REQUIRE_EQ(v2.size(), 2);
                REQUIRE_EQ(v2.capacity(), 2);

                auto v3{move(v)};
                REQUIRE_EQ(allocations, sizeof(int) * 2 * 2);
                REQUIRE_EQ(A_def_ctors, 1);
                REQUIRE_EQ(A_copy_ctors, 1);
                REQUIRE_EQ(A_move_ctors, 1);
                REQUIRE_EQ(A_dtors, 0);
                REQUIRE_EQ(v.size(), 0);
                REQUIRE_EQ(v.capacity(), 0);
                REQUIRE_EQ(v3.size(), 2);
                REQUIRE_EQ(v3.capacity(), 2);
            }
            REQUIRE_EQ(allocations, 0);
            REQUIRE_EQ(A_def_ctors, 1);
            REQUIRE_EQ(A_copy_ctors, 1);
            REQUIRE_EQ(A_move_ctors, 1);
            REQUIRE_EQ(A_dtors, 3);
        }
        SUBCASE("dori::vector follows allocator replacement rules")
        {
            auto f = []<size_t I>() {
                using Al_pocca = bool_constant<static_cast<bool>(I & 0b1)>;
                using Al_pocma = bool_constant<static_cast<bool>(I & 0b10)>;
                using Al_pocs  = bool_constant<static_cast<bool>(I & 0b100)>;
                using Al_iae   = bool_constant<static_cast<bool>(I & 0b1000)>;
                using Eq       = bool_constant<static_cast<bool>(I & 0b1'0000)>;
                DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(A)
                struct Al : Swappable<Al>, A {
                    using A::A;
                    using propagate_on_container_copy_assignment = Al_pocca;
                    using propagate_on_container_move_assignment = Al_pocma;
                    using propagate_on_container_swap            = Al_pocs;
                    using is_always_equal                        = Al_iae;
                    using value_type                             = std::byte;
                    bool operator==(const Al &) const { return Eq::value; }
                    bool operator!=(const Al &) const { return !Eq::value; }
                    std::byte *allocate(size_t) { throw std::bad_alloc{}; }
                    void deallocate(void *, size_t) { throw std::bad_alloc{}; }
                };
                {
                    dori::vector<int, Al> v1, v2;
                    // Program shan't not swap unequal container allocators
                    if constexpr (Eq::value || Al_iae::value || Al_pocs::value)
                        swap(v1, v2);
                    v1 = v2;
                    v1 = move(v2);
                }
                REQUIRE_EQ(A_def_ctors, 2);
                REQUIRE_EQ(A_copy_ctors, 0);
                REQUIRE_EQ(A_move_ctors, 0);
                REQUIRE_EQ(A_copy_asgmts,
                           Al_pocca::value & !Al_iae::value & !Eq::value);
                REQUIRE_EQ(A_move_asgmts, Al_pocma::value & !Al_iae::value);
                REQUIRE_EQ(A_dtors, 2);
                REQUIRE_EQ(Al::swaps, Al_pocs::value & !Al_iae::value);
            };
            [&]<size_t... Is>(index_sequence<Is...>)
            {
                (..., f.template operator()<Is>());
            }
            (make_index_sequence<0b1'1111 + 1>{});
        }
    }

    TEST_CASE("dori::vector supports exceptions")
    {
        SUBCASE("throwing default ctor causes unwind")
        {
            static int until_throw = 9;
            static int dtors       = 0;
            static int ctors       = 0;
            struct S {
                struct error {
                };
                S()
                {
                    if (!until_throw--)
                        throw error{};
                    ++ctors;
                }
                ~S() { ++dtors; }
            };
            dori::vector<S> v;
            v.reserve(10);
            REQUIRE_THROWS_AS(v.resize(10), S::error);
            REQUIRE_EQ(ctors, 9);
            REQUIRE_EQ(dtors, 9);
        }
        SUBCASE("throwing in emplace() of unary tuple does not unwind")
        {
            static int until_throw = 9;
            static int dtors       = 0;
            static int ctors       = 0;
            struct S {
                struct error {
                };
                S()
                {
                    if (!until_throw--)
                        throw error{};
                    ++ctors;
                }
                ~S() { ++dtors; }
            };
            {
                dori::vector<S> v;
                v.reserve(10);
                v.resize(9);
                REQUIRE_THROWS_AS(v.emplace_back(), S::error);
            }
            REQUIRE_EQ(ctors, 9);
            REQUIRE_EQ(dtors, 9);
        }
        SUBCASE("throwing in emplace() of n-ary tuple unwinds")
        {
            DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(A)
            DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(B)
            DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(C)
            DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(D)
            static int until_throw = 2;
            struct error {
            };
            auto get_s = [&]<class Error, class T>() {
                static auto &until_throw_ = until_throw;
                struct S : T {
                    S()
                    {
                        if (!until_throw_--)
                            throw Error{};
                    }
                };
                return S{};
            };
            using SA = decltype(get_s.template operator()<error, A>());
            using SB = decltype(get_s.template operator()<error, B>());
            using SC = decltype(get_s.template operator()<error, C>());
            using SD = decltype(get_s.template operator()<error, D>());
            dori::vector<SA, SB, SC, SD> v;
            v.reserve(1);
            REQUIRE_THROWS_AS(v.emplace_back(), error);
            REQUIRE_EQ(A_def_ctors + B_def_ctors + C_def_ctors + D_def_ctors,
                       3);
            REQUIRE_EQ(A_dtors + B_dtors + C_dtors + D_dtors, 2);
        }
        SUBCASE("throwing from copy ctor unwinds")
        {
            static int until_throw = 9;
            static int dtors       = 0;
            static int def_ctors   = 0;
            static int copy_ctors  = 0;
            struct S {
                struct error {
                };
                S() { ++def_ctors; }
                S(const S &)
                {
                    if (!until_throw--)
                        throw error{};
                    ++copy_ctors;
                }
                ~S() { ++dtors; }
            };
            dori::vector<S> v;
            v.reserve(10);
            v.resize(10);
            REQUIRE_EQ(def_ctors, 10);
            REQUIRE_EQ(copy_ctors, 0);
            REQUIRE_EQ(dtors, 0);
            REQUIRE_THROWS_AS(dori::vector<S>{v}, S::error);
            REQUIRE_EQ(def_ctors, 10);
            REQUIRE_EQ(copy_ctors, 9);
            REQUIRE_EQ(dtors, 9);
        }
    }

    TEST_CASE("dori::vector::shrink_to_fit works")
    {
        DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(A)
        static_assert(!is_trivially_copy_constructible_v<A>);
        {
            dori::vector<A> v;
            v.reserve(10);
            v.resize(8);
            REQUIRE_EQ(A_def_ctors, 8);
            REQUIRE_EQ(A_move_ctors, 0);
            REQUIRE_EQ(A_copy_ctors, 0);
            REQUIRE_EQ(A_dtors, 0);
            REQUIRE_EQ(v.size(), 8);
            REQUIRE_EQ(v.capacity(), 10);
            v.shrink_to_fit();
            REQUIRE_EQ(A_def_ctors, 8);
            REQUIRE_EQ(A_move_ctors, 8);
            REQUIRE_EQ(A_copy_ctors, 0);
            REQUIRE_EQ(A_dtors, 8);
            REQUIRE_EQ(v.size(), 8);
            REQUIRE_EQ(v.capacity(), 8);
        }
        REQUIRE_EQ(A_def_ctors, 8);
        REQUIRE_EQ(A_move_ctors, 8);
        REQUIRE_EQ(A_copy_ctors, 0);
        REQUIRE_EQ(A_dtors, 16);
    }

    TEST_CASE("dori::vector::clear works")
    {
        DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(A);
        {
            dori::vector<A> v;
            v.reserve(10);
            v.clear();
            REQUIRE_EQ(A_def_ctors, 0);
            REQUIRE_EQ(A_copy_ctors, 0);
            REQUIRE_EQ(A_move_ctors, 0);
            REQUIRE_EQ(A_dtors, 0);
            REQUIRE_EQ(v.size(), 0);
            REQUIRE_EQ(v.capacity(), 10);
            v.resize(8);
            v.clear();
            REQUIRE_EQ(A_def_ctors, 8);
            REQUIRE_EQ(A_copy_ctors, 0);
            REQUIRE_EQ(A_move_ctors, 0);
            REQUIRE_EQ(A_dtors, 8);
            REQUIRE_EQ(v.size(), 0);
            REQUIRE_EQ(v.capacity(), 10);
        }
        REQUIRE_EQ(A_def_ctors, 8);
        REQUIRE_EQ(A_copy_ctors, 0);
        REQUIRE_EQ(A_move_ctors, 0);
        REQUIRE_EQ(A_dtors, 8);
    }
}