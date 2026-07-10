/**
 * @file    goliath_engine/common/guest_memory.h
 * @license BSD 3-Clause, see LICENSE
 */
#pragma once

#include <rex/types.h>
#if __has_include(<rex/kernel/kernel_state.h>)
#include <rex/kernel/kernel_state.h>
#define EOT_REX_HAS_KERNEL_NAMESPACE 1
#else
#include <rex/system/kernel_state.h>
#define EOT_REX_HAS_KERNEL_NAMESPACE 0
#endif

namespace eot::guest {

using Address = rex::guest_addr_t;

using U8 = rex::be_u8;
using U16 = rex::be_u16;
using U32 = rex::be_u32;
using U64 = rex::be_u64;
using I8 = rex::be_i8;
using I16 = rex::be_i16;
using I32 = rex::be_i32;
using I64 = rex::be_i64;
using F32 = rex::be_f32;
using F64 = rex::be_f64;

namespace detail {

inline rex::memory::Memory* Memory() {
#if EOT_REX_HAS_KERNEL_NAMESPACE
  auto* kernel_state = rex::kernel::kernel_state();
#else
  auto* kernel_state = REX_KERNEL_STATE();
#endif
  return kernel_state ? kernel_state->memory() : nullptr;
}

}  // namespace detail

inline rex::memory::Memory* Memory() { return detail::Memory(); }

inline rex::u8* Base() {
  auto* memory = Memory();
  return memory ? memory->virtual_membase() : nullptr;
}

template <typename T = rex::u8*>
inline T ToHost(Address address) {
  auto* memory = Memory();
  return (memory && address) ? memory->TranslateVirtual<T>(address) : nullptr;
}

template <typename T>
inline T Read(Address address, T fallback = {}) {
  const auto* value = ToHost<const rex::be<T>*>(address);
  return value ? static_cast<T>(*value) : fallback;
}

template <typename T>
inline void Write(Address address, T value) {
  if (auto* dst = ToHost<rex::be<T>*>(address)) {
    *dst = value;
  }
}

inline void OrU32(Address address, rex::u32 mask) {
  Write<rex::u32>(address, Read<rex::u32>(address) | mask);
}

}  // namespace eot::guest

namespace eot {

inline rex::u8* GuestBase() { return guest::Base(); }

// Mark an X360 task dead (flags |= 0xDEAD0000 at +0x58, destroyFlag=1 at +0x60).
inline void KillGuestTask(guest::Address task_address) {
  if (!task_address) return;
  guest::Write<rex::u32>(task_address + 0x60, 1u);
  guest::OrU32(task_address + 0x58, 0xDEAD0000u);
}

inline void KillGuestTask(rex::u8* base, guest::Address task_address) {
  (void)base;
  KillGuestTask(task_address);
}

}  // namespace eot

#undef EOT_REX_HAS_KERNEL_NAMESPACE
