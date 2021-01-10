#include <dori/all.h>
#include <numeric>
#include <optional>
#include <stdint.h>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

TEST_SUITE("dori::vector")
{
#define DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(X)                           \
    static int X##_def_ctors  = 0;                                             \
    static int X##_move_ctors = 0;                                             \
    static int X##_copy_ctors = 0;                                             \
    static int X##_dtors      = 0;                                             \
    struct X {                                                                 \
        X() noexcept { ++X##_def_ctors; }                                      \
        X &operator=(const X &rhs) = delete;                                   \
        X &operator=(X &&rhs) = delete;                                        \
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
        SUBCASE("contained elements are tightly packed")
        {
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
        static std::size_t nctors = 0;
        static std::size_t nmoves = 0;
        static std::size_t ndtors = 0;
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
        v.erase(v.begin(), std::next(v.begin(), 4));
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
        using It      = decltype(v)::iterator;
        const auto eq = [](auto a, auto b) {
            return std::get<0>(a) == std::get<0>(b);
        };
        const auto even = [](auto x) { return std::get<0>(x) % 2 == 0; };

        v.reserve(8);
        std::tuple<int> nums[]{{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}};
        std::copy(&nums[0], &nums[8], std::back_inserter(v));
        REQUIRE(std::equal<It>(v.begin(), v.end(), &nums[0], eq));

        v.erase(std::remove_if<It>(v.begin(), v.end(), even), v.end());
        std::tuple<int> odds[]{{1}, {3}, {5}, {7}};
        REQUIRE_EQ(v.size(), std::size(odds));
        REQUIRE(std::equal<It>(v.begin(), v.end(), odds, eq));

        decltype(v) v2;
        v2.reserve(v.size());
        std::transform<It>(v.begin(), v.end(), odds, std::back_inserter(v2),
                           [](auto a, auto b) -> std::tuple<int> {
                               return {std::get<0>(a) + std::get<0>(b)};
                           });
        std::tuple<int> odds_doubled[]{{2}, {6}, {10}, {14}};
        REQUIRE_EQ(v2.size(), std::size(odds_doubled));
        REQUIRE(std::equal<It>(v2.begin(), v2.end(), odds_doubled, eq));

        const int sum = std::reduce<It>(
            ++v2.begin(), v2.end(), std::get<0>(v2[0]),
            [](int acc, auto cur) { return acc + std::get<0>(cur); });
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
                auto v3 = std::move(v);
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
            DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(C)
            DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(D)
            {
                dori::vector<A, B> v;
                v.reserve(8);
                v.resize(8);
                auto v2 = dori::vector_cast<C, D>(v);
            }
            REQUIRE_EQ(A_def_ctors, 8);
            REQUIRE_EQ(B_def_ctors, 8);
            REQUIRE_EQ(C_copy_ctors, 8);
            REQUIRE_EQ(D_copy_ctors, 8);
            REQUIRE_EQ(A_dtors, 8);
            REQUIRE_EQ(B_dtors, 8);
            REQUIRE_EQ(C_dtors, 8);
            REQUIRE_EQ(D_dtors, 8);
        }
    }

    TEST_CASE("dori::vector orders element sequences stably")
    {
        dori::vector<int, float, int64_t, double> v;
        auto &cast = dori::vector_cast<float, std::array<char, 4>, double,
                                       std::array<char, 8>>(v);
        v.reserve(1);
        v.push_back({10, 20.f, 30i64, 40.});
        auto [v0, v1, v2, v3] = v[0];
        auto [c0, c1, c2, c3] = cast[0];
        REQUIRE_EQ(v0, std::bit_cast<int>(c0));
        REQUIRE_EQ(v1, std::bit_cast<float>(c1));
        REQUIRE_EQ(v2, std::bit_cast<int64_t>(c2));
        REQUIRE_EQ(v3, std::bit_cast<double>(c3));
    }

#define DORI_VECTOR_TEST_EXPLICIT_NEW_AND_DELETE_BEGIN                         \
    __pragma(warning(push)) __pragma(warning(disable : 26409))
#define DORI_VECTOR_TEST_EXPLICIT_NEW_AND_DELETE_END __pragma(warning(pop))

    TEST_CASE("dori::vector satisfies AllocatorAwareContainer")
    {
        DORI_VECTOR_TEST_DEFINE_CTOR_DTOR_COUNTER(A)
        static std::size_t allocations = 0;
        struct Al : A {
            using A::A;
            using is_always_equal = std::true_type;
            using value_type      = char;
            bool operator==(const Al &) const { return true; }
            bool operator!=(const Al &) const { return false; }
            char *allocate(std::size_t n)
            {
                allocations += n;
                DORI_VECTOR_TEST_EXPLICIT_NEW_AND_DELETE_BEGIN
                return new char[n];
                DORI_VECTOR_TEST_EXPLICIT_NEW_AND_DELETE_END
            }
            void deallocate(void *p, std::size_t n) noexcept
            {
                allocations -= n;
                DORI_VECTOR_TEST_EXPLICIT_NEW_AND_DELETE_BEGIN
                delete[](char *) p;
                DORI_VECTOR_TEST_EXPLICIT_NEW_AND_DELETE_END
            }
        };
        {
            dori::vector_al<Al, int> v;
            using V = decltype(v);
#define LOGIC_check_expr(r, E, ...)                                            \
    static_assert(                                                             \
        std::is_invocable_r_v<r, decltype(DORI_f_ref(E)), __VA_ARGS__>);

            static_assert(std::is_same_v<V::allocator_type, Al>);
            LOGIC_check_expr(Al, v.get_allocator);
            LOGIC_check_expr(V &, v.operator=, V &);
            LOGIC_check_expr(V &, v.operator=, const V &&);
            LOGIC_check_expr(V &, v.operator=, V &&);
            LOGIC_check_expr(void, v.swap, V &);

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

            auto v3{std::move(v)};
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
            REQUIRE_THROWS_AS(v.resize(10), S::error);
            REQUIRE_EQ(ctors, 9);
            REQUIRE_EQ(dtors, 9);
        }
        SUBCASE("throwing in emplace() of unary tuple does nothing")
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
            static bool do_throw = true;
            struct S {
                struct error {
                };
                S()
                {
                    if (do_throw)
                        throw error{};
                }
            };
            dori::vector<A, B, S, C, D> v;
            v.reserve(1);
            REQUIRE_THROWS_AS(v.emplace_back(), S::error);
            REQUIRE_EQ(A_def_ctors, 1);
            REQUIRE_EQ(B_def_ctors, 1);
            REQUIRE_EQ(C_def_ctors, 0);
            REQUIRE_EQ(D_def_ctors, 0);
            REQUIRE_EQ(A_dtors, 1);
            REQUIRE_EQ(B_dtors, 1);
            REQUIRE_EQ(C_dtors, 0);
            REQUIRE_EQ(D_dtors, 0);
            SUBCASE("... emplacees only")
            {
                do_throw = false;
                v.reserve(2);
                v.resize(1);
                do_throw = true;
                REQUIRE_THROWS_AS(v.emplace_back(), S::error);
                REQUIRE_EQ(A_def_ctors, 1 + 2);
                REQUIRE_EQ(B_def_ctors, 1 + 2);
                REQUIRE_EQ(C_def_ctors, 0 + 1);
                REQUIRE_EQ(D_def_ctors, 0 + 1);
                REQUIRE_EQ(A_dtors, 1 + 1);
                REQUIRE_EQ(B_dtors, 1 + 1);
                REQUIRE_EQ(C_dtors, 0 + 0);
                REQUIRE_EQ(D_dtors, 0 + 0);
            }
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
        static_assert(!std::is_trivially_copy_constructible_v<A>);
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