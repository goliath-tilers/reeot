/**
 * @file    goliath_engine/common/hooks.h
 * @brief   Macros for marking recompiled functions as intentional no-ops.
 *
 * @copyright Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *            All rights reserved.
 * @license   BSD 3-Clause License
 *            See LICENSE file in the project root for full license text.
 * @edit      Modifications Copyright (c) 2026 Rien Gupta <rgupta9@scu.edu>
 *            All rights reserved.
 */
#pragma once

#include <rex/hook.h>

// Intentional no-op with no log output, for calls the host need not model.
// (REX_STUB warns on every call; use it for unimplemented placeholders.)
#define EOT_NOOP(subroutine)         \
  extern "C" REX_FUNC(subroutine) { \
    (void)ctx;                      \
    (void)base;                     \
  }

// EOT_NOOP that returns a constant u32 in r3, for callers comparing the result.
#define EOT_NOOP_RETURN(subroutine, value) \
  extern "C" REX_FUNC(subroutine) {       \
    (void)base;                           \
    ctx.r3.u64 = (value);                 \
  }
