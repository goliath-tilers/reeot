/**
 * @file    goliath_engine/common/encoding.h
 * @license BSD 3-Clause, see LICENSE
 */
#pragma once
#include <string>
#include <string_view>
namespace eot {
std::wstring Utf8ToWide(std::string_view s);
std::string  WideToUtf8(std::wstring_view s);
std::string  SjisToUtf8(std::string_view s);
std::string  U16ToUtf8(std::u16string_view s);
}
