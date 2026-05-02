#pragma once

#include <utility>

namespace fuzzy {

template <typename F>
class DeferGuard {
 public:
  explicit DeferGuard(F&& fn) :
      fn_(std::forward<F>(fn)) {}
  ~DeferGuard() noexcept(std::is_nothrow_invocable_v<F>) {
    fn_();
  }

 private:
  F fn_;
};

namespace detail {
struct defer_guard_tag {};

template <typename F>
DeferGuard<F> operator+(detail::defer_guard_tag, F&& fn) {
  return DeferGuard<std::decay_t<F>>{std::forward<F>(fn)};
}
}  // namespace detail
}  // namespace fuzzy

#define FUZZY_DEFER auto FUZZY_ANON_VAR(defer_guard_) = ::fuzzy::detail::defer_guard_tag{} + [&]

#define FUZZY_STRCAT(a, b) _FUZZY_STRCAT_INTERNAL(a, b)
#define _FUZZY_STRCAT_INTERNAL(a, b) a##b

#ifdef __COUNTER__
#  define FUZZY_ANON_VAR(prefix) FUZZY_STRCAT(prefix, __COUNTER__)
#else
#  define FUZZY_ANON_VAR(prefix) FUZZY_STRCAT(prefix, __LINE__)
#endif
