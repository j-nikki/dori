#pragma once

#include "inline.h"

namespace dori::detail
{

template <class F, class... Args>
constexpr DORI_inline void Call_maybe_unsafe(F &&f, Args &&...args) noexcept
{
    //
    // Core Guidelines E.12: Use noexcept when exiting a function because of
    // a throw is impossible or unacceptable
    //
    // This function is used when a possibly not-noexcept invocable is called
    // and the handling of any thrown exceptions is not supported (for example
    // move assignment, move construction, destruction...).
    //
#if DORI_DEBUG
    try {
#endif
        static_cast<F &&>(f)(static_cast<Args &&>(args)...);
#if DORI_DEBUG
    } catch (...) {
        DORI_assert(!"the dori library doesn't support this operation "
                     "throwing - please make your operation non-throwing");
    }
#endif
}

} // namespace dori::detail