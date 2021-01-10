# dori

This is a C++ container library concerned with data-oriented design. Currently only tested on VS 2019 Preview on x86 and x86-64. The code is constexpr and noexcept where possible. Unhappy paths are approached space-economically.

## Runthrough

`dori::vector<Ts...> v` arranges the contained data so that all objects of each type are stored contiguously in the program memory. It is also quaranteed that only one allocation is made to contain all the elements and that the amount of memory reserved is no greater than `v.capacity() * (sizeof(Ts) + ...)`.

To visualize, consider `dori::vector<int32_t, int8_t, int64_t>`. Internally the element sequences are ordered descendingly by alignment so that, given `size() == capacity() == 4`, the internal layout is `i64 i64 i64 i64 i32 i32 i32 i32 i8 i8 i8 i8`.

Some differences to `std::vector`: `reference` is a tuple of lvalue references, `push_back()`, `emplace_back()`, and `resize()` assume sufficient space, the vector never shrinks of its own accord (use `shrink_to_fit()` or `v = {}` to shrink or empty).

`dori::vector_cast<Us...>(v)` is a utility function that provides a reinterpreted view to the elements of the target vector. The order among element sequences of same size is stable: given `dori::vector<double, int, float> v`, `dori::vector_cast<float, int, double>(v)` will provide such view to `v` that the ints and floats shall be interpreted as floats and ints respectively.

## Using in your project

Please see `LICENSE` for terms of use.

The easiest way to use dori in your project is to include it as a submodule: `git submodule add git@github.com:j-nikki/dori.git`. Now discover dori in your CMake file (so you can `#include <dori/...>` in C++):
```cmake
add_subdirectory("dori")
target_link_libraries(<target-name> ... dori)
```
