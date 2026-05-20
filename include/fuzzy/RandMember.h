#ifndef FUZZY_RAND_MEMBER_H
#define FUZZY_RAND_MEMBER_H

#include <concepts>
#include <type_traits>

namespace fuzzy {

/// Optional scalar holder with SystemVerilog-like `rand_mode(int)` on the member object.
/// Use `FUZZY_META(..., FUZZY_RANDM(x), ...)` so the library binds `VarExpr` to `value` and
/// `&fuzzy_rand_mode_` (no separate `__fuzzy_rm_x` flag is generated).
template <std::integral T>
struct RandMember {
  T value{};
  bool fuzzy_rand_mode_ = true;

  void rand_mode(int m) {
    fuzzy_rand_mode_ = (m != 0);
  }

  T& ref() {
    return value;
  }
  const T& ref() const {
    return value;
  }
};

}  // namespace fuzzy

#endif  // FUZZY_RAND_MEMBER_H
