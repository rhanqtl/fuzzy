#ifndef FUZZY_ASSERT_H
#define FUZZY_ASSERT_H

#include <cstdlib>
#include <format>
#include <iostream>
#include <ostream>
#include <source_location>
#include <string_view>

namespace fuzzy::detail {
[[noreturn]] inline void handle_assert(const std::source_location& loc, std::string_view expr) {
  std::cerr << std::format("Assertion '{}' failed at {}:{}", expr, loc.file_name(), loc.line())
            << std::endl;
  std::abort();
}

template <typename... Args>
[[noreturn]] inline void handle_unreachable(const std::source_location& loc, std::string_view fmt,
                                            Args&&... args) {
  std::string msg;
  if (fmt.empty()) {
    msg = std::format("Unreachable reached at {}:{}", loc.file_name(), loc.line());
  } else {
    msg = std::format("Unreachable reached at {}:{}: ", loc.file_name(), loc.line());
    msg += std::vformat(fmt, std::make_format_args(args...));
  }
  std::cerr << msg << std::endl;
  std::abort();
}

template <typename... Args>
[[noreturn]] inline void handle_todo(const std::source_location& loc, std::string_view fmt,
                                     Args&&... args) {
  std::string msg;
  if (fmt.empty()) {
    msg = std::format("Unimplemented reached at {}:{}", loc.file_name(), loc.line());
  } else {
    msg = std::format("Unimplemented reached at {}:{}: ", loc.file_name(), loc.line());
    msg += std::vformat(fmt, std::make_format_args(args...));
  }
  std::cerr << msg << std::endl;
  std::abort();
}
}  // namespace fuzzy::detail

#define fuzzy_assert(expr)                                                    \
  do {                                                                        \
    if (!(expr))                                                              \
      ::fuzzy::detail::handle_assert(std::source_location::current(), #expr); \
  } while (false);

#define fuzzy_unreachable(fmt, ...)                                                           \
  do {                                                                                        \
    ::fuzzy::detail::handle_unreachable(std::source_location::current(), fmt, ##__VA_ARGS__); \
  } while (false);

#define fuzzy_todo(fmt, ...)                                                           \
  do {                                                                                 \
    ::fuzzy::detail::handle_todo(std::source_location::current(), fmt, ##__VA_ARGS__); \
  } while (false);

#endif  // FUZZY_ASSERT_H
