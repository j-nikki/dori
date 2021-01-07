# dori

This is a C++ container library concerned with data-oriented design.

## Runthrough

`v = dori::vector<Ts...>()` arranges the contained data so that all objects of type T<sub>i</sub> are stored contiguously in the program memory, where i ∈ { 0, 1, `sizeof...(Ts)-1` }. It is also quaranteed that only one allocation is made to contain all the elements and that the amount of memory reserved is no greater than `v.capacity() * std::max({sizeof(Ts)...})`.

`dori::vector_cast<Us...>(v)` is a utility function that provides a reinterpreted view to the elements of the target vector.

## Using in your project

Please see `LICENSE` for terms of use.

The easiest way to use iface in your project is to include it as a submodule: `git submodule add git@github.com:j-nikki/dori.git`. Now discover dori in your CMake file (so you can `#include <dori.h>` in C++):
```cmake
add_subdirectory("dori")
target_link_libraries(<target-name> ... dori)
```
