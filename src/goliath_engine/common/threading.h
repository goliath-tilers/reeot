/**
 * @file    goliath_engine/common/threading.h
 * @brief   Native threading, sleep, and frame timing hooks.
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

void EnableHighResTimer();
void DisableHighResTimer();

// TerminateProcess and spin; never returns, runs no destructors (not a clean exit).
[[noreturn]] void TerminateProcessNow();

} // namespace eot
