//          _____                            _   _
//         / ____|                          | | (_)
//        | (___   ___ _ __ ___   __ _ _ __ | |_ _  ___
//         \___ \ / _ \ '_ ` _ \ / _` | '_ \| __| |/ __|
//         ____) |  __/ | | | | | (_| | | | | |_| | (__
//        |_____/ \___|_| |_| |_|\__,_|_| |_|\__|_|\___|
// __      __           _             _                _____
// \ \    / /          (_)           (_)              / ____|_     _
//  \ \  / /__ _ __ ___ _  ___  _ __  _ _ __   __ _  | |   _| |_ _| |_
//   \ \/ / _ \ '__/ __| |/ _ \| '_ \| | '_ \ / _` | | |  |_   _|_   _|
//    \  /  __/ |  \__ \ | (_) | | | | | | | | (_| | | |____|_|   |_|
//     \/ \___|_|  |___/_|\___/|_| |_|_|_| |_|\__, |  \_____|
// https://github.com/Neargye/semver           __/ |
// version 0.2.2                              |___/
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2018 - 2020 Daniil Goncharov <neargye@gmail.com>.
//
// Permission is hereby  granted, free of charge, to any  person obtaining a copy
// of this software and associated  documentation files (the "Software"), to deal
// in the Software  without restriction, including without  limitation the rights
// to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
// copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
// IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
// FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
// AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
// LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef NEARGYE_SEMANTIC_VERSIONING_HPP
#define NEARGYE_SEMANTIC_VERSIONING_HPP

#include <cstdint>
#include <cstddef>
#include <limits>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#if __has_include(<charconv>)
#include <charconv>
#else
#include <system_error>
#endif

// Allow to disable exceptions.
#if (defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)) && !defined(SEMVER_NOEXCEPTION)
#  include <stdexcept>
#  define __SEMVER_THROW(exception) throw exception
#else
#  include <cstdlib>
#  define __SEMVER_THROW(exception) std::abort()
#endif

#if defined(__APPLE_CC__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wmissing-braces" // Ignore warning: suggest braces around initialization of subobject 'return {first, std::errc::invalid_argument};'.
#endif

namespace semver {

enum struct prerelease : std::uint8_t {
  alpha = 0,
  beta = 1,
  rc = 2,
  none = 3
};

// Max version string length = 3(<major>) + 1(.) + 3(<minor>) + 1(.) + 3(<patch>) + 1(-) + 5(<prerelease>) + 1(.) + 3(<prereleaseversion>) = 21.
inline constexpr std::size_t max_version_string_length = 21;

namespace detail {

#if __has_include(<charconv>)
struct from_chars_result : std::from_chars_result {
  [[nodiscard]] constexpr operator bool() const noexcept { return ec == std::errc{}; }
};

struct to_chars_result : std::to_chars_result {
  [[nodiscard]] constexpr operator bool() const noexcept { return ec == std::errc{}; }
};
#else
struct from_chars_result {
  const char* ptr;
  std::errc ec;

  [[nodiscard]] constexpr operator bool() const noexcept { return ec == std::errc{}; }
};

struct to_chars_result {
  char* ptr;
  std::errc ec;

  [[nodiscard]] constexpr operator bool() const noexcept { return ec == std::errc{}; }
};
#endif

inline constexpr std::string_view alpha = {"-alpha", 6};
inline constexpr std::string_view beta  = {"-beta", 5};
inline constexpr std::string_view rc    = {"-rc", 3};

// Min version string length = 1(<major>) + 1(.) + 1(<minor>) + 1(.) + 1(<patch>) = 5.
inline constexpr auto min_version_string_length = 5;

constexpr char to_lower(char c) noexcept {
  return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

constexpr bool is_digit(char c) noexcept {
  return c >= '0' && c <= '9';
}

constexpr std::uint8_t to_digit(char c) noexcept {
  return c - '0';
}

constexpr std::uint8_t length(std::uint8_t x) noexcept {
  return x < 10 ? 1 : (x < 100 ? 2 : 3);
}

constexpr std::uint8_t length(prerelease t) noexcept {
  if (t == prerelease::alpha) {
    return 5;
  } else if (t == prerelease::beta) {
    return 4;
  } else if(t == prerelease::rc) {
    return 2;
  }

  return 0;
}

constexpr bool equals(const char* first, const char* last, std::string_view str) noexcept {
  for (std::size_t i = 0; first != last && i < str.length(); ++i, ++first) {
    if (to_lower(*first) != to_lower(str[i])) {
      return false;
    }
  }

  return true;
}

constexpr char* to_chars(char* str, std::uint8_t x, bool dot = true) noexcept {
  do {
    *(--str) = static_cast<char>('0' + (x % 10));
    x /= 10;
  } while (x != 0);

  if (dot) {
    *(--str) = '.';
  }

  return str;
}

constexpr char* to_chars(char* str, prerelease t) noexcept {
  const auto p = t == prerelease::alpha
                     ? alpha
                     : t == prerelease::beta
                           ? beta
                           : t == prerelease::rc
                                 ? rc
                                 : std::string_view{};

  for (auto it = p.rbegin(); it != p.rend(); ++it) {
    *(--str) = *it;
  }

  return str;
}

constexpr const char* from_chars(const char* first, const char* last, std::uint8_t& d) noexcept {
  if (first != last && is_digit(*first)) {
    std::int32_t t = 0;
    for (; first != last && is_digit(*first); ++first) {
      t = t * 10 + to_digit(*first);
    }
    if (t <= (std::numeric_limits<std::uint8_t>::max)()) {
      d = static_cast<std::uint8_t>(t);
      return first;
    }
  }

  return nullptr;
}

constexpr const char* from_chars(const char* first, const char* last, prerelease& p) noexcept {
  if (equals(first, last, alpha)) {
    p = prerelease::alpha;
    return first + alpha.length();
  } else if (equals(first, last, beta)) {
    p = prerelease::beta;
    return first + beta.length();
  } else if (equals(first, last, rc)) {
    p = prerelease::rc;
    return first + rc.length();
  }

  return nullptr;
}

constexpr bool check_delimiter(const char* first, const char* last, char d) noexcept {
  return first != last && first != nullptr && *first == d;
}

} // namespace semver::detail

struct version {
  std::uint8_t major             = 0;
  std::uint8_t minor             = 1;
  std::uint8_t patch             = 0;
  prerelease prerelease_type     = prerelease::none;
  std::uint8_t prerelease_number = 0;

  constexpr version(std::uint8_t major,
                    std::uint8_t minor,
                    std::uint8_t patch,
                    prerelease prerelease_type = prerelease::none,
                    std::uint8_t prerelease_number = 0) noexcept
      : major{major},
        minor{minor},
        patch{patch},
        prerelease_type{prerelease_type},
        prerelease_number{prerelease_type == prerelease::none ? static_cast<std::uint8_t>(0) : prerelease_number} {
  }

  explicit constexpr version(std::string_view str) : version(0, 0, 0, prerelease::none, 0) {
    from_string(str);
  }

  constexpr version() = default; // https://semver.org/#how-should-i-deal-with-revisions-in-the-0yz-initial-development-phase

  constexpr version(const version&) = default;

  constexpr version(version&&) = default;

  ~version() = default;

  version& operator=(const version&) = default;

  version& operator=(version&&) = default;

  [[nodiscard]] constexpr detail::from_chars_result from_chars(const char* first, const char* last) noexcept {
    if (first == nullptr || last == nullptr || (last - first) < detail::min_version_string_length) {
      return {{first, std::errc::invalid_argument}};
    }

    auto next = first;
    if (next = detail::from_chars(next, last, major); detail::check_delimiter(next, last, '.')) {
      if (next = detail::from_chars(++next, last, minor); detail::check_delimiter(next, last, '.')) {
        if (next = detail::from_chars(++next, last, patch); next == last) {
          prerelease_type = prerelease::none;
          prerelease_number = 0;
          return {{next, std::errc{}}};
        } else if (detail::check_delimiter(next, last, '-')) {
          if (next = detail::from_chars(next, last, prerelease_type); next == last) {
            prerelease_number = 0;
            return {{next, std::errc{}}};
          } else if (detail::check_delimiter(next, last, '.')) {
            if (next = detail::from_chars(++next, last, prerelease_number); next == last) {
              return {{next, std::errc{}}};
            }
          }
        }
      }
    }

    return {{first, std::errc::invalid_argument}};
  }

  [[nodiscard]] constexpr detail::to_chars_result to_chars(char* first, char* last) const noexcept {
    const auto length = chars_length();
    if (first == nullptr || last == nullptr || (last - first) < length) {
      return {{last, std::errc::value_too_large}};
    }

    auto next = first + length;
    if (prerelease_type != prerelease::none) {
      if (prerelease_number != 0) {
        next = detail::to_chars(next, prerelease_number);
      }
      next = detail::to_chars(next, prerelease_type);
    }
    next = detail::to_chars(next, patch);
    next = detail::to_chars(next, minor);
    next = detail::to_chars(next, major, false);

    return {{first + length, std::errc{}}};
  }

  [[nodiscard]] constexpr bool from_string_noexcept(std::string_view str) noexcept {
    return from_chars(str.data(), str.data() + str.length());
  }

  constexpr version& from_string(std::string_view str) {
    if (!from_string_noexcept(str)) {
      __SEMVER_THROW(std::invalid_argument{"semver::version::from_string invalid version."});
    }

    return *this;
  }

  [[nodiscard]] std::string to_string() const {
    auto str = std::string(chars_length(), '\0');
    if (!to_chars(str.data(), str.data() + str.length())) {
      __SEMVER_THROW(std::invalid_argument{"semver::version::to_string invalid version."});
    }

    return str;
  }

  [[nodiscard]] constexpr int compare(const version& other) const noexcept {
    if (major != other.major) {
      return major - other.major;
    }

    if (minor != other.minor) {
      return minor - other.minor;
    }

    if (patch != other.patch) {
      return patch - other.patch;
    }

    if (prerelease_type != other.prerelease_type) {
      return static_cast<std::uint8_t>(prerelease_type) - static_cast<std::uint8_t>(other.prerelease_type);
    }

    if (prerelease_number != other.prerelease_number) {
      return prerelease_number - other.prerelease_number;
    }

    return 0;
  }

 private:
  constexpr std::uint8_t chars_length() const noexcept {
    // (<major>) + 1(.) + (<minor>) + 1(.) + (<patch>)
    std::uint8_t length = detail::length(major) + detail::length(minor) + detail::length(patch) + 2;
    if (prerelease_type != prerelease::none) {
      // + 1(-) + (<prerelease>)
      length += detail::length(prerelease_type) + 1;
      if (prerelease_number != 0) {
        // + 1(.) + (<prereleaseversion>)
        length += detail::length(prerelease_number) + 1;
      }
    }
    return length;
  }
};

[[nodiscard]] constexpr bool operator==(const version& lhs, const version& rhs) noexcept {
  return lhs.compare(rhs) == 0;
}

[[nodiscard]] constexpr bool operator!=(const version& lhs, const version& rhs) noexcept {
  return lhs.compare(rhs) != 0;
}

[[nodiscard]] constexpr bool operator>(const version& lhs, const version& rhs) noexcept {
  return lhs.compare(rhs) > 0;
}

[[nodiscard]] constexpr bool operator>=(const version& lhs, const version& rhs) noexcept {
  return lhs.compare(rhs) >= 0;
}

[[nodiscard]] constexpr bool operator<(const version& lhs, const version& rhs) noexcept {
  return lhs.compare(rhs) < 0;
}

[[nodiscard]] constexpr bool operator<=(const version& lhs, const version& rhs) noexcept {
  return lhs.compare(rhs) <= 0;
}

[[nodiscard]] constexpr version operator""_version(const char* str, std::size_t length) {
  return version{std::string_view{str, length}};
}

[[nodiscard]] constexpr bool valid(std::string_view str) noexcept {
  return version{}.from_string_noexcept(str);
}

[[nodiscard]] constexpr detail::from_chars_result from_chars(const char* first, const char* last, version& v) noexcept {
  return v.from_chars(first, last);
}

[[nodiscard]] constexpr detail::to_chars_result to_chars(char* first, char* last, const version& v) noexcept {
  return v.to_chars(first, last);
}

[[nodiscard]] constexpr std::optional<version> from_string_noexcept(std::string_view str) noexcept {
  if (auto v = version{}; v.from_string_noexcept(str)) {
    return v;
  }

  return std::nullopt;
}

[[nodiscard]] constexpr version from_string(std::string_view str) {
  return version{str};
}

[[nodiscard]] inline std::string to_string(const version& v) {
  return v.to_string();
}

template <typename Char, typename Traits>
inline std::basic_ostream<Char, Traits>& operator<<(std::basic_ostream<Char, Traits>& os, const version& v) {
  for (const auto c : v.to_string()) {
    os.put(c);
  }

  return os;
}

namespace ranges {

} // namespace semver::ranges

} // namespace semver

#if defined(__APPLE_CC__)
#  pragma clang diagnostic pop
#endif

#endif // NEARGYE_SEMANTIC_VERSIONING_HPP