/**
 * @file    goliath_engine/common/reboot.cpp
 * @brief   Warm reboot: persist settings, quiesce the guest, relaunch the exe.
 *
 * @copyright Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *            All rights reserved.
 * @license   BSD 3-Clause License
 *            See LICENSE file in the project root for full license text.
 * @edit      Modifications Copyright (c) 2026 Rien Gupta <rgupta9@scu.edu>
 *            All rights reserved.
 */
#include "goliath_engine/common/reboot.h"
#include "goliath_engine/common/logging.h"

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <string>

#include <rex/cvar.h>
#include <rex/logging.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace eot {
namespace {

std::function<void()> g_handler;
std::mutex g_handler_mutex;
std::atomic<bool> g_requested{false};
std::string g_active_profile;
std::filesystem::path g_config_path;

std::filesystem::path ExecutablePath() {
  wchar_t buf[MAX_PATH];
  DWORD n = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
  return std::filesystem::path(buf, buf + n);
}

bool SpawnFreshInstance(const std::filesystem::path &exe) {
  std::wstring cmdline = L"\"" + exe.wstring() + L"\"";
  if (!g_active_profile.empty()) {
    std::wstring wprofile(g_active_profile.begin(), g_active_profile.end());
    cmdline += L" --profile \"" + wprofile + L"\"";
  }
  std::wstring workdir = exe.parent_path().wstring();

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};

  if (!::CreateProcessW(exe.c_str(), cmdline.data(), nullptr, nullptr, FALSE, 0,
                        nullptr, workdir.c_str(), &si, &pi)) {
    EOT_ERROR("[reboot] CreateProcessW failed (err {})", ::GetLastError());
    return false;
  }
  ::CloseHandle(pi.hThread);
  ::CloseHandle(pi.hProcess);
  return true;
}

} // namespace

std::filesystem::path ConfigFilePath() {
  if (!g_config_path.empty()) return g_config_path;
  return ExecutablePath().parent_path() / "reblue.toml";
}

void SetProfileContext(std::string profile_name,
                       std::filesystem::path config_path) {
  g_active_profile = std::move(profile_name);
  g_config_path = std::move(config_path);
}

void SetWarmRebootHandler(std::function<void()> handler) {
  std::lock_guard<std::mutex> lk(g_handler_mutex);
  g_handler = std::move(handler);
}

void RequestWarmReboot() {
  bool expected = false;
  if (!g_requested.compare_exchange_strong(expected, true))
    return;

  std::function<void()> handler;
  {
    std::lock_guard<std::mutex> lk(g_handler_mutex);
    handler = g_handler;
  }
  if (!handler) {
    EOT_ERROR("[reboot] no handler registered; cannot relaunch");
    g_requested.store(false);
    return;
  }
  EOT_WARN("[reboot] warm reboot requested");
  handler();
}

void PerformWarmReboot(const std::function<void()> &quiesce) {
  auto exe = ExecutablePath();

  // 1. Settings are not auto-persisted; write them before relaunch.
  rex::cvar::SaveConfig(ConfigFilePath());

  // 2. Spawn first: if it fails, stay in the running session rather than kill
  //    it with no replacement.
  if (!SpawnFreshInstance(exe)) {
    EOT_ERROR("[reboot] relaunch failed; staying in current session");
    g_requested.store(false);
    return;
  }

  // 3. Cooperatively stop guest threads so none is mid-I/O (no force-kill).
  if (quiesce)
    quiesce();

  // 4. Flush the writers we keep; no subsystem teardown (it would deadlock on a
  //    host lock a straggler still holds). The OS reclaims the rest.
  rex::FlushLogging();

  // 5. Exit without running destructors.
  std::_Exit(0);
}

} // namespace eot
