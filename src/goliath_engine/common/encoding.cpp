/**
 * @file    goliath_engine/common/encoding.cpp
 * @license BSD 3-Clause, see LICENSE
 */
#include "goliath_engine/common/encoding.h"
#include <rex/platform.h>
#if REX_PLATFORM_WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
namespace eot {
std::wstring Utf8ToWide(std::string_view s) {
  if (s.empty()) return {};
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
  std::wstring o((size_t)n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), o.data(), n);
  return o;
}
std::string WideToUtf8(std::wstring_view s) {
  if (s.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
  std::string o((size_t)n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), o.data(), n, nullptr, nullptr);
  return o;
}
std::string SjisToUtf8(std::string_view s) {
  if (s.empty()) return {};
  int n = MultiByteToWideChar(932, 0, s.data(), (int)s.size(), nullptr, 0);
  if (n <= 0) return std::string(s);
  std::wstring w((size_t)n, L'\0');
  MultiByteToWideChar(932, 0, s.data(), (int)s.size(), w.data(), n);
  return WideToUtf8(w);
}
std::string U16ToUtf8(std::u16string_view s) {
  if (s.empty()) return {};
  static_assert(sizeof(wchar_t) == sizeof(char16_t));
  return WideToUtf8(std::wstring_view(reinterpret_cast<const wchar_t*>(s.data()), s.size()));
}
}  // namespace eot
#endif
