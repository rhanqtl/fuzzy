#include "fuzzy/ast/Expr.h"

#include <cassert>
#include <memory>
#include <vector>

namespace fuzzy {

// ---------- Thread-local arena ----------
// ConstraintCollector sets this before symbolic execution, clears it after.

namespace detail {

static thread_local Arena* current_arena = nullptr;

void set_arena(Arena* arena) {
  current_arena = arena;
}
Arena* get_arena() {
  return current_arena;
}

template <typename T, typename... Args>
T& arena_alloc(Args&&... args) {
  assert(current_arena && "Operator used outside of constraint collection context");
  auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
  T& ref = *ptr;
  current_arena->nodes.push_back(std::move(ptr));
  return ref;
}

Expr& make_binary(ExprKind kind, Expr& lhs, Expr& rhs) {
  return arena_alloc<BinaryExpr>(kind, &lhs, &rhs);
}

Expr& make_unary(ExprKind kind, Expr& operand) {
  return arena_alloc<UnaryExpr>(kind, &operand);
}

Expr& make_const(int64_t value) {
  return arena_alloc<ConstExpr>(value);
}

Expr& make_ite(Expr& cond, Expr& then_expr, Expr& else_expr) {
  return arena_alloc<IteExpr>(&cond, &then_expr, &else_expr);
}

Expr& make_call(std::vector<Expr*> args, CallExpr::EvalFn eval_fn) {
  return arena_alloc<CallExpr>(std::move(args), std::move(eval_fn));
}

}  // namespace detail

// ---------- Expr & Expr operators ----------

Expr& operator+(Expr& lhs, Expr& rhs) {
  return detail::make_binary(ExprKind::Add, lhs, rhs);
}

Expr& operator-(Expr& lhs, Expr& rhs) {
  return detail::make_binary(ExprKind::Sub, lhs, rhs);
}

Expr& operator*(Expr& lhs, Expr& rhs) {
  return detail::make_binary(ExprKind::Mul, lhs, rhs);
}

Expr& operator/(Expr& lhs, Expr& rhs) {
  return detail::make_binary(ExprKind::Div, lhs, rhs);
}

Expr& operator%(Expr& lhs, Expr& rhs) {
  return detail::make_binary(ExprKind::Mod, lhs, rhs);
}

Expr& operator<(Expr& lhs, Expr& rhs) {
  return detail::make_binary(ExprKind::Less, lhs, rhs);
}

Expr& operator<=(Expr& lhs, Expr& rhs) {
  return detail::make_binary(ExprKind::LessEqual, lhs, rhs);
}

Expr& operator>(Expr& lhs, Expr& rhs) {
  return detail::make_binary(ExprKind::Greater, lhs, rhs);
}

Expr& operator>=(Expr& lhs, Expr& rhs) {
  return detail::make_binary(ExprKind::GreaterEqual, lhs, rhs);
}

Expr& operator==(Expr& lhs, Expr& rhs) {
  return detail::make_binary(ExprKind::Equal, lhs, rhs);
}

Expr& operator!=(Expr& lhs, Expr& rhs) {
  return detail::make_binary(ExprKind::NotEqual, lhs, rhs);
}

Expr& operator&&(Expr& lhs, Expr& rhs) {
  return detail::make_binary(ExprKind::LogicalAnd, lhs, rhs);
}

Expr& operator||(Expr& lhs, Expr& rhs) {
  return detail::make_binary(ExprKind::LogicalOr, lhs, rhs);
}

// ---------- int & Expr / Expr & int operators ----------

Expr& operator+(int64_t lhs, Expr& rhs) {
  return detail::make_binary(ExprKind::Add, detail::make_const(lhs), rhs);
}
Expr& operator+(Expr& lhs, int64_t rhs) {
  return detail::make_binary(ExprKind::Add, lhs, detail::make_const(rhs));
}

Expr& operator-(int64_t lhs, Expr& rhs) {
  return detail::make_binary(ExprKind::Sub, detail::make_const(lhs), rhs);
}
Expr& operator-(Expr& lhs, int64_t rhs) {
  return detail::make_binary(ExprKind::Sub, lhs, detail::make_const(rhs));
}

Expr& operator*(int64_t lhs, Expr& rhs) {
  return detail::make_binary(ExprKind::Mul, detail::make_const(lhs), rhs);
}
Expr& operator*(Expr& lhs, int64_t rhs) {
  return detail::make_binary(ExprKind::Mul, lhs, detail::make_const(rhs));
}

Expr& operator/(int64_t lhs, Expr& rhs) {
  return detail::make_binary(ExprKind::Div, detail::make_const(lhs), rhs);
}
Expr& operator/(Expr& lhs, int64_t rhs) {
  return detail::make_binary(ExprKind::Div, lhs, detail::make_const(rhs));
}

Expr& operator%(int64_t lhs, Expr& rhs) {
  return detail::make_binary(ExprKind::Mod, detail::make_const(lhs), rhs);
}
Expr& operator%(Expr& lhs, int64_t rhs) {
  return detail::make_binary(ExprKind::Mod, lhs, detail::make_const(rhs));
}

Expr& operator<(int64_t lhs, Expr& rhs) {
  return detail::make_binary(ExprKind::Less, detail::make_const(lhs), rhs);
}
Expr& operator<(Expr& lhs, int64_t rhs) {
  return detail::make_binary(ExprKind::Less, lhs, detail::make_const(rhs));
}

Expr& operator<=(int64_t lhs, Expr& rhs) {
  return detail::make_binary(ExprKind::LessEqual, detail::make_const(lhs), rhs);
}
Expr& operator<=(Expr& lhs, int64_t rhs) {
  return detail::make_binary(ExprKind::LessEqual, lhs, detail::make_const(rhs));
}

Expr& operator>(int64_t lhs, Expr& rhs) {
  return detail::make_binary(ExprKind::Greater, detail::make_const(lhs), rhs);
}
Expr& operator>(Expr& lhs, int64_t rhs) {
  return detail::make_binary(ExprKind::Greater, lhs, detail::make_const(rhs));
}

Expr& operator>=(int64_t lhs, Expr& rhs) {
  return detail::make_binary(ExprKind::GreaterEqual, detail::make_const(lhs), rhs);
}
Expr& operator>=(Expr& lhs, int64_t rhs) {
  return detail::make_binary(ExprKind::GreaterEqual, lhs, detail::make_const(rhs));
}

Expr& operator==(int64_t lhs, Expr& rhs) {
  return detail::make_binary(ExprKind::Equal, detail::make_const(lhs), rhs);
}
Expr& operator==(Expr& lhs, int64_t rhs) {
  return detail::make_binary(ExprKind::Equal, lhs, detail::make_const(rhs));
}

Expr& operator!=(int64_t lhs, Expr& rhs) {
  return detail::make_binary(ExprKind::NotEqual, detail::make_const(lhs), rhs);
}
Expr& operator!=(Expr& lhs, int64_t rhs) {
  return detail::make_binary(ExprKind::NotEqual, lhs, detail::make_const(rhs));
}

// ---------- Unary ----------

Expr& operator!(Expr& operand) {
  return detail::make_unary(ExprKind::LogicalNot, operand);
}

Expr& abs(Expr& operand) {
  return detail::make_unary(ExprKind::Abs, operand);
}

}  // namespace fuzzy
