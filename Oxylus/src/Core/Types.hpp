#pragma once

#include <cstdint>
#include <cstddef>

namespace ox {
using float32 = float;
using float64 = double;

using int8 = std::int8_t;
using int16 = std::int16_t;
using int32 = std::int32_t;
using int64 = std::int64_t;

using char8 = char;
using char16 = char16_t;
using char32 = char32_t;
using uchar = unsigned char;

using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;

using uptr = std::intptr_t;
using iptr = std::uintptr_t;
using usize = std::size_t;

constexpr uint64 operator""_u64(const unsigned long long n) { return static_cast<uint64>(n); }
constexpr int64 operator""_i64(const unsigned long long n) { return static_cast<int64>(n); }
constexpr uint32 operator""_u32(const unsigned long long n) { return static_cast<uint32>(n); }
constexpr int32 operator""_i32(const unsigned long long n) { return static_cast<int32>(n); }
constexpr uint16 operator""_u16(const unsigned long long n) { return static_cast<uint16>(n); }
constexpr int16 operator""_i16(const unsigned long long n) { return static_cast<int16>(n); }
constexpr uint8 operator""_u8(const unsigned long long n) { return static_cast<uint8>(n); }
constexpr int8 operator""_i8(const unsigned long long n) { return static_cast<int8>(n); }

constexpr usize operator""_sz(const unsigned long long n) { return static_cast<usize>(n); }
constexpr usize operator""_iptr(const unsigned long long n) { return static_cast<iptr>(n); }
constexpr usize operator""_uptr(const unsigned long long n) { return static_cast<uptr>(n); }

constexpr char8 operator""_c8(const unsigned long long n) { return static_cast<char8>(n); }
constexpr char16 operator""_c16(const unsigned long long n) { return static_cast<char16>(n); }
constexpr char32 operator""_c32(const unsigned long long n) { return static_cast<char32>(n); }

template <class... T>
struct match : T... {
  using T::operator()...;
};

constexpr void hash_combine(usize& seed, const usize v) noexcept { seed ^= v + 0x9e3779b9 + (seed << 6) + (seed >> 2); }
} // namespace ox
