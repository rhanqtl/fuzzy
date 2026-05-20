#ifndef FUZZY_DSL_CONTEXT_H
#define FUZZY_DSL_CONTEXT_H

#include <concepts>
#include <cstdint>
#include <initializer_list>
#include <type_traits>
#include <utility>

#include "fuzzy/ConstraintCollector.h"
#include "fuzzy/ast/Expr.h"
#include "fuzzy/ast/Func.h"
#include "fuzzy/ast/RandArray.h"

namespace fuzzy::dsl {

/// Inclusive `[lo:hi]` range for SV-style `inside { ..., [lo:hi], ... }`.
struct InsideSpan {
  int64_t lo;
  int64_t hi;
};

constexpr InsideSpan inside_span(int64_t lo, int64_t hi) {
  return {lo, hi};
}

/// Binds DSL verbs to a `ConstraintCollector` without embedding that logic in preprocessor macros.
/// Macro-generated code should only forward to these members so the surface syntax can evolve
/// independently from the AST / solver pipeline.
class DslContext {
 public:
  explicit DslContext(ConstraintCollector& cc) :
      cc_{cc} {}

  template <typename T>
  void require(T&& e) {
    if constexpr (std::derived_from<std::remove_reference_t<T>, Expr>) {
      cc_.add_constraint(e);
    } else if constexpr (std::same_as<std::remove_cvref_t<T>, bool>) {
      if (!e) {
        cc_.add_constraint(cc_.make_false());
      }
    }
  }

  template <typename Arr, typename Fn>
  void for_each_i(Arr& arr, Fn&& fn) {
    cc_.for_each_i(arr, std::forward<Fn>(fn));
  }

  template <typename Map, typename Fn>
  void for_each_kv(Map& m, Fn&& fn) {
    cc_.for_each_kv(m, std::forward<Fn>(fn));
  }

  template <typename First, typename... Rest>
  void unique(First& first, Rest&... rest) {
    if constexpr (sizeof...(rest) == 0 &&
                  std::derived_from<std::remove_reference_t<First>, ArrayExprBase>) {
      cc_.add_unique(static_cast<ArrayExprBase&>(first));
    } else {
      cc_.add_unique(first, rest...);
    }
  }

  void soft(Expr& e) {
    cc_.add_soft_constraint(e);
  }

  void disable_soft(Expr& e) {
    cc_.disable_soft_constraint(e);
  }

  void solve_before(Expr& lhs, Expr& rhs) {
    cc_.add_solve_before(lhs, rhs);
  }

  template <typename ExprLike>
  void dist(ExprLike& expr, std::initializer_list<std::pair<int64_t, int>> values) {
    cc_.add_dist(expr, values);
  }

  Expr& implies(Expr& cond, Expr& body) {
    return cc_.make_implies(cond, body);
  }

  template <typename F>
  void constraint_if(Expr& cond, F&& then_fn) {
    cc_.constraint_if(cond, std::forward<F>(then_fn));
  }

  template <typename F, typename G>
  void constraint_if_else(Expr& cond, F&& then_fn, G&& else_fn) {
    cc_.constraint_if_else(cond, std::forward<F>(then_fn), std::forward<G>(else_fn));
  }

  /// SV `inside { v1, v2, [lo:hi], ... }` as a single disjunctive membership constraint.
  template <typename First, typename... Rest>
  void inside(Expr& x, First&& first, Rest&&... rest) {
    Expr* cur = &inside_atom(x, std::forward<First>(first));
    ((cur = &detail::make_binary(ExprKind::LogicalOr, *cur, inside_atom(x, std::forward<Rest>(rest)))),
     ...);
    cc_.add_constraint(*cur);
  }

  template <std::integral T>
  Expr& array_sum(ArrayExpr<T>& arr) {
    return cc_.make_array_agg(ArrayAggKind::Sum, arr);
  }

  template <std::integral T, typename F>
  Expr& array_sum(ArrayExpr<T>& arr, F&& with_fn) {
    return cc_.make_array_agg(ArrayAggKind::Sum, arr, std::forward<F>(with_fn));
  }

  template <std::integral T>
  Expr& array_product(ArrayExpr<T>& arr) {
    return cc_.make_array_agg(ArrayAggKind::Product, arr);
  }

  template <std::integral T, typename F>
  Expr& array_product(ArrayExpr<T>& arr, F&& with_fn) {
    return cc_.make_array_agg(ArrayAggKind::Product, arr, std::forward<F>(with_fn));
  }

  template <std::integral T>
  Expr& array_and(ArrayExpr<T>& arr) {
    return cc_.make_array_agg(ArrayAggKind::And, arr);
  }

  template <std::integral T, typename F>
  Expr& array_and(ArrayExpr<T>& arr, F&& with_fn) {
    return cc_.make_array_agg(ArrayAggKind::And, arr, std::forward<F>(with_fn));
  }

  template <std::integral T>
  Expr& array_or(ArrayExpr<T>& arr) {
    return cc_.make_array_agg(ArrayAggKind::Or, arr);
  }

  template <std::integral T, typename F>
  Expr& array_or(ArrayExpr<T>& arr, F&& with_fn) {
    return cc_.make_array_agg(ArrayAggKind::Or, arr, std::forward<F>(with_fn));
  }

  template <std::integral T>
  Expr& array_xor(ArrayExpr<T>& arr) {
    return cc_.make_array_agg(ArrayAggKind::Xor, arr);
  }

  template <std::integral T, typename F>
  Expr& array_xor(ArrayExpr<T>& arr, F&& with_fn) {
    return cc_.make_array_agg(ArrayAggKind::Xor, arr, std::forward<F>(with_fn));
  }

  template <std::integral T>
  Expr& array_min(ArrayExpr<T>& arr) {
    return cc_.make_array_agg(ArrayAggKind::Min, arr);
  }

  template <std::integral T, typename F>
  Expr& array_min(ArrayExpr<T>& arr, F&& with_fn) {
    return cc_.make_array_agg(ArrayAggKind::Min, arr, std::forward<F>(with_fn));
  }

  template <std::integral T>
  Expr& array_max(ArrayExpr<T>& arr) {
    return cc_.make_array_agg(ArrayAggKind::Max, arr);
  }

  template <std::integral T, typename F>
  Expr& array_max(ArrayExpr<T>& arr, F&& with_fn) {
    return cc_.make_array_agg(ArrayAggKind::Max, arr, std::forward<F>(with_fn));
  }

  template <typename Self, typename Fn, typename... Args>
  Expr& invoke(Self& self, const Fn& fn, Args&&... args) {
    return ::fuzzy::dsl::invoke(self, fn, std::forward<Args>(args)...);
  }

  Expr& bit_select(Expr& x, unsigned bit) {
    return bit_slice(x, bit, bit);
  }

  Expr& bit_slice(Expr& x, unsigned hi, unsigned lo) {
    const int64_t divisor = pow2(lo);
    const int64_t width = static_cast<int64_t>(hi - lo + 1);
    const int64_t modulus = pow2(static_cast<unsigned>(width));
    return detail::make_binary(
        ExprKind::Mod,
        detail::make_binary(ExprKind::Div, x, detail::make_const(divisor)),
        detail::make_const(modulus));
  }

  Expr& bit_concat(Expr& hi, unsigned lo_width, Expr& lo) {
    auto& shifted_hi = detail::make_binary(ExprKind::Mul, hi, detail::make_const(pow2(lo_width)));
    return detail::make_binary(ExprKind::Add, shifted_hi, lo);
  }

  Expr& wildcard_eq(Expr& value, int64_t pattern, int64_t care_mask) {
    Expr* cur = nullptr;
    for (unsigned bit = 0; bit < 63; ++bit) {
      const int64_t bit_mask = int64_t{1} << bit;
      if ((care_mask & bit_mask) == 0) {
        continue;
      }
      auto& eq = detail::make_binary(ExprKind::Equal, bit_select(value, bit),
                                     detail::make_const((pattern & bit_mask) != 0));
      cur = cur ? &detail::make_binary(ExprKind::LogicalAnd, *cur, eq) : &eq;
    }
    return cur ? *cur : detail::make_const(1);
  }

  Expr& wildcard_ne(Expr& value, int64_t pattern, int64_t care_mask) {
    return detail::make_unary(ExprKind::LogicalNot, wildcard_eq(value, pattern, care_mask));
  }

  ConstraintCollector& collector() {
    return cc_;
  }

 private:
  template <std::integral T>
  static Expr& inside_atom(Expr& x, T v) {
    return detail::make_binary(ExprKind::Equal, x, detail::make_const(static_cast<int64_t>(v)));
  }

  static Expr& inside_atom(Expr& x, InsideSpan s) {
    auto& ge = detail::make_binary(ExprKind::GreaterEqual, x, detail::make_const(s.lo));
    return detail::make_binary(ExprKind::LogicalAnd, ge,
                               detail::make_binary(ExprKind::LessEqual, x, detail::make_const(s.hi)));
  }

  static int64_t pow2(unsigned bits) {
    return int64_t{1} << bits;
  }

  ConstraintCollector& cc_;
};

}  // namespace fuzzy::dsl

#endif  // FUZZY_DSL_CONTEXT_H
