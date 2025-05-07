#pragma once

namespace ox {
using f32 = float;
using f64 = double;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using c8 = char;
using c16 = char16_t;
using c32 = char32_t;
using b32 = u32;

using uptr = std::intptr_t;
using iptr = std::uintptr_t;
using usize = std::size_t;

constexpr u64 operator""_u64(const unsigned long long n) { return static_cast<u64>(n); }
constexpr i64 operator""_i64(const unsigned long long n) { return static_cast<i64>(n); }
constexpr u32 operator""_u32(const unsigned long long n) { return static_cast<u32>(n); }
constexpr i32 operator""_i32(const unsigned long long n) { return static_cast<i32>(n); }
constexpr u16 operator""_u16(const unsigned long long n) { return static_cast<u16>(n); }
constexpr i16 operator""_i16(const unsigned long long n) { return static_cast<i16>(n); }
constexpr u8 operator""_u8(const unsigned long long n) { return static_cast<u8>(n); }
constexpr i8 operator""_i8(const unsigned long long n) { return static_cast<i8>(n); }

constexpr usize operator""_sz(const unsigned long long n) { return static_cast<usize>(n); }
constexpr usize operator""_iptr(const unsigned long long n) { return static_cast<iptr>(n); }
constexpr usize operator""_uptr(const unsigned long long n) { return static_cast<uptr>(n); }

constexpr c8 operator""_c8(const unsigned long long n) { return static_cast<c8>(n); }
constexpr c16 operator""_c16(const unsigned long long n) { return static_cast<c16>(n); }
constexpr c32 operator""_c32(const unsigned long long n) { return static_cast<c32>(n); }

template <typename T>
constexpr T align_up(T size,
                     u64 alignment) {
  return T((u64(size) + (alignment - 1)) & ~(alignment - 1));
}

template <typename T>
constexpr T align_down(T size,
                       u64 alignment) {
  return T(u64(size) & ~(alignment - 1));
}

template <typename T>
constexpr T kib_to_bytes(const T x) {
  return x << static_cast<T>(10);
}

template <typename T>
constexpr T mib_to_bytes(const T x) {
  return x << static_cast<T>(20);
}

template <class... T>
struct match : T... {
  using T::operator()...;
};

template <typename T,
          usize N>
constexpr usize count_of(T (&)[N]) {
  return N;
}

constexpr void hash_combine(usize& seed,
                            const usize v) noexcept {
  seed ^= v + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}
} // namespace ox
