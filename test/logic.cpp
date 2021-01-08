#include <dori/all.h>
#include <numeric>
#include <optional>

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
        auto v = dori::vector<A, B>();

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
            auto v = dori::vector<A, B>();
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
        auto v = dori::vector<A, B>();

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
        auto v = dori::vector<int8_t, int64_t, int16_t, int32_t>();
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
            S(S&&) { ++nmoves; }
            S &operator=(S &&) { return ++nmoves, *this; }
            ~S() { ++ndtors; }
        };
        auto v = dori::vector<S>();
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
        auto v          = dori::vector<int>();
        using It        = decltype(v)::iterator;
        const auto eq   = [](auto a, int b) { return std::get<0>(a) == b; };
        const auto even = [](auto x) { return std::get<0>(x) % 2 == 0; };

        v.reserve(8);
        int nums[]{0, 1, 2, 3, 4, 5, 6, 7};
        std::copy(&nums[0], &nums[8], std::back_inserter(v));
        REQUIRE(std::equal<It>(v.begin(), v.end(), nums, eq));

        v.erase(std::remove_if<It>(v.begin(), v.end(), even), v.end());
        int odds[]{1, 3, 5, 7};
        REQUIRE_EQ(v.size(), std::size(odds));
        REQUIRE(std::equal<It>(v.begin(), v.end(), odds, eq));

        decltype(v) v2;
        v2.reserve(v.size());
        std::transform<It>(v.begin(), v.end(), odds, std::back_inserter(v2),
                           [](auto a, int b) { return std::get<0>(a) + b; });
        int odds_doubled[]{2, 6, 10, 14};
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
        auto v = dori::vector<A, B>();
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

    TEST_CASE("dori::vector_cast works")
    {
        SUBCASE("result of dori::vector_cast uses its own offset array")
        {
            auto v   = dori::vector<int, double>();
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
                auto v = dori::vector<A, B>();
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

#define DORI_VECTOR_TEST_EXPLICIT_NEW_AND_DELETE_BEGIN                         \
    __pragma(warning(push)) __pragma(warning(disable : 26409))
#define DORI_VECTOR_TEST_EXPLICIT_NEW_AND_DELETE_END __pragma(warning(pop))

    TEST_CASE("dori::vector supports custom allocators")
    {
        static std::size_t copy_ctors  = 0;
        static std::size_t def_ctors   = 0;
        static std::size_t dtors       = 0;
        static std::size_t allocations = 0;
        struct allocator {
            using value_type = char;
            allocator() noexcept { ++def_ctors; }
            allocator(allocator &&) = delete;
            allocator &operator=(const allocator &) = delete;
            allocator &operator=(allocator &&) = delete;
            allocator(const allocator &) noexcept { ++copy_ctors; }
            ~allocator() { ++dtors; }
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
        } al;
        {
            auto v = dori::vector<int>(al);
            v.reserve(2);
            v.resize(2);
            REQUIRE_EQ(allocations, v.capacity() * sizeof(int));

            auto v2 = v;
            REQUIRE_EQ(allocations,
                       (v.capacity() + v2.capacity()) * sizeof(int));
        }
        REQUIRE_EQ(def_ctors, 1);
        REQUIRE_EQ(copy_ctors, 2);
        REQUIRE_EQ(dtors, 2);
        REQUIRE_EQ(allocations, 0);
    }
}