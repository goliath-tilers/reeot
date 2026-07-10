/**
 * @file    goliath_engine/common/threading.cpp
 * @brief   Native threading, sleep, and frame timing hooks.
 *
 * @copyright Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *            All rights reserved.
 * @license   BSD 3-Clause License
 *            See LICENSE file in the project root for full license text.
 * @edit      Modifications Copyright (c) 2026 Rien Gupta <rgupta9@scu.edu>
 *            All rights reserved.
 */


#include "goliath_engine/common/threading.h"

#include <chrono>
#include <immintrin.h>
#include <mutex>
#include <thread>

#include "goliath_engine/common/logging.h"

#include <rex/types.h>
#include <rex/ppc.h>
#include <rex/system/xthread.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <timeapi.h>

namespace {
std::once_flag g_timer_init;
}

namespace eot {

void EnableHighResTimer() {
    std::call_once(g_timer_init, [] {
        timeBeginPeriod(1);
    });
}

void DisableHighResTimer() {
    timeEndPeriod(1);
}

void TerminateProcessNow() {
    ::TerminateProcess(::GetCurrentProcess(), 0);
    // TerminateProcess can return before the caller halts; spin so nothing runs past here.
    for (;;) {
        _mm_pause();
    }
}

} // namespace eot

// Sleep() - PPC kernel bypass.
u32 Sleep_hook(u32 ms) {
    eot::EnableHighResTimer();

    if (ms == 0) {
        SwitchToThread();
        return 0;
    }

    auto target = std::chrono::steady_clock::now()
                + std::chrono::milliseconds(u32(ms));

    if (ms >= 2) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(u32(ms)) - std::chrono::microseconds(1500));
    } else {
        SwitchToThread();
    }

    while (std::chrono::steady_clock::now() < target)
        _mm_pause();

    return 0;
}
REX_HOOK(eot_Sleep, Sleep_hook);
