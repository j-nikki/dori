# dori

This is a C++ container library concerned with data-oriented design.

## Runthrough

`v = dori::vector<Ts...>()` arranges the contained data so that all objects of each type are stored contiguously in the program memory. It is also quaranteed that only one allocation is made to contain all the elements and that the amount of memory reserved is no greater than `v.capacity() * (sizeof(Ts) + ...)`.

To visualize, consider `dori::vector<int32_t, int64_t, int8_t>`. Internally the element sequences are ordered descendingly by alignment, so that, for a vector of `size() == capacity() == 4`, the internal layout is `i64 i64 i64 i64 i32 i32 i32 i32 i8 i8 i8 i8`.

`dori::vector_cast<Us...>(v)` is a utility function that provides a reinterpreted view to the elements of the target vector.

## Using in your project

Please see `LICENSE` for terms of use.

The easiest way to use dori in your project is to include it as a submodule: `git submodule add git@github.com:j-nikki/dori.git`. Now discover dori in your CMake file (so you can `#include <dori.h>` in C++):
```cmake
add_subdirectory("dori")
target_link_libraries(<target-name> ... dori)
```
