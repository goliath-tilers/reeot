/**
 * @file    goliath_engine/common/crash_handler.cpp
 * @brief   See header.
 *
 * @copyright Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *            All rights reserved.
 * @license   BSD 3-Clause License
 *            See LICENSE file in the project root for full license text.
 * @edit      Modifications Copyright (c) 2026 Rien Gupta <rgupta9@scu.edu>
 *            All rights reserved.
 */
#include "goliath_engine/common/crash_handler.h"

#include <atomic>
#include <cstdint>

#include <rex/exception_handler.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/version.h>

#include "goliath_engine/common/logging.h"

// Linker-provided base of THIS module (reblue.exe). Its ADDRESS is the post-ASLR
// load base; using it avoids including Windows.h (project rule) while letting us
// reduce a faulting host PC to an RVA that the .map/.pdb resolves offline.
extern "C" const unsigned char __ImageBase;

// Exported by kernel32/ntdll. Declared locally to avoid pulling in Windows.h; x64
// has a single calling convention so __stdcall is a no-op there.
extern "C" __declspec(dllimport) unsigned short __stdcall RtlCaptureStackBackTrace(
    unsigned long frames_to_skip, unsigned long frames_to_capture, void** back_trace,
    unsigned long* back_trace_hash);

namespace eot {
namespace {

// Guest virtual address space is the low 4 GB of the host mapping.
constexpr uint64_t kGuestAddressSpaceSize = 0x100000000ull;

// Generous upper bound on the reblue.exe image span (it embeds all recompiled guest
// code, so it is large). Used only to decide whether a host address reduces to an
// in-module RVA; oversizing it never misclassifies an in-module address.
constexpr uint64_t kHostImageSpan = 0x20000000ull;  // 512 MiB

uint64_t HostModuleBase() { return reinterpret_cast<uint64_t>(&__ImageBase); }

const char* ExceptionCodeName(rex::arch::Exception::Code code) {
  using Code = rex::arch::Exception::Code;
  switch (code) {
    case Code::kAccessViolation:    return "ACCESS_VIOLATION";
    case Code::kIllegalInstruction: return "ILLEGAL_INSTRUCTION";
    default:                        return "UNKNOWN";
  }
}

const char* AvOperationName(rex::arch::Exception::AccessViolationOperation op) {
  using Op = rex::arch::Exception::AccessViolationOperation;
  switch (op) {
    case Op::kRead:  return "read";
    case Op::kWrite: return "write";
    default:         return "unknown";
  }
}

// Walk the faulting thread's stack (the VEH runs inline on it) and log each return
// address with its reblue.exe-relative RVA. The faulting PC is already logged
// separately; this gives the call chain that led there. The top frames are the
// crash handler itself - scan for the first reblue.exe+RVA below the OS dispatch
// frames to find the faulting caller.
void LogBacktrace(uint64_t base) {
  void* frames[32] = {};
  const unsigned short n = RtlCaptureStackBackTrace(0, 32, frames, nullptr);
  if (!n) return;
  EOT_CRITICAL("backtrace ({} frames; top frames are the crash handler):", n);
  for (unsigned short i = 0; i < n; ++i) {
    const uint64_t a = reinterpret_cast<uint64_t>(frames[i]);
    if (a >= base && a - base < kHostImageSpan) {
      EOT_CRITICAL("    [{:>2}] {:#018x}  (reblue.exe+{:#010x})", i, a, a - base);
    } else {
      EOT_CRITICAL("    [{:>2}] {:#018x}", i, a);
    }
  }
}

// Read the register file straight off the public HostThreadContext members -
// the SDK's GetRegisterName/GetStringFromValue helpers are header-declared but
// not exported by the runtime lib, so format the values here instead.
void LogRegisters(const rex::arch::HostThreadContext* ctx) {
  if (!ctx) return;
#if REX_ARCH_AMD64
  static const char* const kNames[16] = {
      "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
      "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15"};
  EOT_CRITICAL("    rip = {:#018x}   eflags = {:#010x}", ctx->rip, ctx->eflags);
  for (int i = 0; i < 16; ++i) {
    EOT_CRITICAL("    {:>3} = {:#018x}", kNames[i], ctx->int_registers[i]);
  }
#elif REX_ARCH_ARM64
  EOT_CRITICAL("    pc = {:#018x}   sp = {:#018x}   pstate = {:#010x}", ctx->pc,
              ctx->sp, ctx->pstate);
  for (int i = 0; i < 31; ++i) {
    EOT_CRITICAL("    x{:<2} = {:#018x}", i, ctx->x[i]);
  }
#endif
}

// Chained after the SDK MMIO handler; only sees faults the runtime declined.
// Returns false: never recover, let the OS produce its default termination.
bool CrashHandler(rex::arch::Exception* ex, void* /*data*/) {
  // A fault inside the handler must not loop; bail straight to the OS.
  static std::atomic_flag s_in_handler = ATOMIC_FLAG_INIT;
  if (s_in_handler.test_and_set(std::memory_order_acq_rel)) return false;

  const uint64_t host_base = HostModuleBase();

  EOT_CRITICAL("================ reblue host crash ================");
  EOT_CRITICAL("build: {}", REXGLUE_BUILD_TITLE);
  EOT_CRITICAL("module base: reblue.exe @ {:#018x}", host_base);
  EOT_CRITICAL("exception: {} @ host pc {:#018x}",
              ExceptionCodeName(ex->code()), ex->pc());
  if (ex->pc() >= host_base && ex->pc() - host_base < kHostImageSpan) {
    EOT_CRITICAL("faulting RVA: reblue.exe+{:#010x}", ex->pc() - host_base);
  }

  if (ex->code() == rex::arch::Exception::Code::kAccessViolation) {
    const uint64_t fa = ex->fault_address();
    EOT_CRITICAL("fault address: {:#018x} ({})", fa,
                 AvOperationName(ex->access_violation_operation()));

    // Map a guest-range fault back to its guest VA - the actionable triage line
    // for a fault inside recompiled code.
    auto* rt = rex::Runtime::instance();
    uint8_t* membase = rt ? rt->virtual_membase() : nullptr;
    const uint64_t base = reinterpret_cast<uint64_t>(membase);
    if (membase && fa >= base && fa < base + kGuestAddressSpaceSize) {
      EOT_CRITICAL("guest fault VA: {:#010x}", static_cast<uint32_t>(fa - base));
    }
  }

  EOT_CRITICAL("registers:");
  LogRegisters(ex->thread_context());
  LogBacktrace(host_base);
  EOT_CRITICAL("===================================================");

  // Drain sinks before the OS tears us down; a crash bypasses OnShutdown.
  rex::FlushLogging();
  return false;
}

}  // namespace

void InstallCrashHandler() {
  rex::arch::ExceptionHandler::Install(&CrashHandler, nullptr);
  EOT_INFO("[crash] last-chance handler installed");
}

}  // namespace eot
