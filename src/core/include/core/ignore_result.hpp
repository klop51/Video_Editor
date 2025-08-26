// Utility macro to explicitly ignore a [[nodiscard]] result in a self-documenting way.
// Prefer handling return values; only use for intentional one-off discards.
#pragma once

#define VE_IGNORE_RESULT(expr) do { auto _ve_tmp = (expr); (void)_ve_tmp; } while(0)
