/**
 * @file    bdengine/common/reboot.h
 * @brief   Clean warm reboot (host process relaunch) for applying restart-bound
 *          settings and DLC changes.
 *
 * @copyright Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *            All rights reserved.
 * @license   BSD 3-Clause License
 *            See LICENSE file in the project root for full license text.
 * @edit      Modifications Copyright (c) 2026 Rien Gupta <rgupta9@scu.edu>
 *            All rights reserved.
 */
#pragma once

#include <filesystem>
#include <functional>
#include <string>

namespace eot {

// Where cvar settings are persisted: the active profile's reblue.toml once set
// via SetProfileContext, else <exe_dir>/reblue.toml (pre-profile fallback).
std::filesystem::path ConfigFilePath();

// Record the active profile so warm reboot persists to the profile's config and
// re-passes --profile to the fresh instance (SpawnFreshInstance forwards no
// other args). Call once during path setup.
void SetProfileContext(std::string profile_name,
                       std::filesystem::path config_path);

// Registered once by the host app. Marshals the relaunch onto the UI thread,
// where the kernel state is reachable (as in ReXApp::OnClosing).
void SetWarmRebootHandler(std::function<void()> handler);

// Request a clean warm reboot. Safe to call from a guest thread; idempotent.
// Invokes the registered host handler, which relaunches the process.
void RequestWarmReboot();

// Persist cvars, quiesce the guest (via `quiesce`), flush logs, spawn a fresh
// instance, and exit this one. UI thread only. Returns only if the relaunch
// could not be started (current session is left untouched).
void PerformWarmReboot(const std::function<void()> &quiesce);

} // namespace eot
