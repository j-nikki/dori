#pragma once

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
#define DORI_inline inline __forceinline
#else
#define DORI_inline inline
#endif