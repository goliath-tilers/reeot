/**
 * @file    goliath_engine/common/crash_handler.h
 * @brief   Last-chance host crash logger: dumps the faulting PC, fault address,
 *          guest VA, and register state to the eot log before the process dies,
 *          so crashes in the wild are triageable.
 *
 * @copyright Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *            All rights reserved.
 * @license   BSD 3-Clause License
 *            See LICENSE file in the project root for full license text.
 * @edit      Modifications Copyright (c) 2026 Rien Gupta <rgupta9@scu.edu>
 *            All rights reserved.
 */
#pragma once

namespace eot {

// Install the last-chance host exception handler. Call once, after the Runtime
// and logging are up: it chains *after* the SDK's guest-MMIO handler, so it only
// fires on faults the runtime did not resolve (i.e. real crashes), logs them via
// EOT_CRITICAL, flushes the log, and returns control to the OS for termination.
void InstallCrashHandler();

}  // namespace eot
