#pragma once

#include "inline.h"

namespace dori::detail
{
template <bool NoThrow, class F, class... Args>
constexpr DORI_inline void Call_maybe_unsafe(std::bool_constant<NoThrow>, F &&f,
                                             Args &&...args) noexcept(NoThrow)
{
    //
    // Core Guidelines E.12: Use noexcept when exiting a function because of
    // a throw is impossible or unacceptable
    //
    if constexpr (NoThrow) {
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
    } else
        static_cast<F &&>(f)(static_cast<Args &&>(args)...);
}
template <class F, class... Args>
requires std::is_invocable_v<F &&, Args &&...> constexpr DORI_inline void
Call_maybe_unsafe(F &&f, Args &&...args) noexcept
{
    Call_maybe_unsafe(std::true_type{}, static_cast<F &&>(f),
                      static_cast<Args &&>(args)...);
}
} // namespace dori::detail