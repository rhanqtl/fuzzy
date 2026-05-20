#ifndef FUZZY_AST_EXPR_H
#define FUZZY_AST_EXPR_H

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fuzzy {

enum class ExprKind {
  // Leaf nodes
  Var,
  Const,

  // Arithmetic (binary)
  Add,
  Sub,
  Mul,
  Div,
  Mod,
  // Arithmetic (unary)
  Abs,

  // Comparison (binary, boolean semantics)
  Less,
  LessEqual,
  Greater,
  GreaterEqual,
  Equal,
  NotEqual,

  // Logical
  LogicalAnd,
  LogicalOr,
  LogicalNot,

  // Constraint-specific
  Implies,
  Ite,
  Call,

  // Array-related
  ArraySize,
  ArrayElem,

  // Higher-order constraints (expanded before reaching Z3)
  ForEachI,
  ForEachKV,
  Unique,

  /// Whole-array aggregate (expanded in Phase 2 after elements materialize)
  ArrayAgg,
};

// ---------- Base class ----------

class Expr {
 public:
  explicit Expr(ExprKind kind) :
      kind_{kind} {}
  virtual ~Expr() = default;

  Expr(const Expr&) = delete;
  Expr& operator=(const Expr&) = delete;

  ExprKind kind() const {
    return kind_;
  }

 private:
  ExprKind kind_;
};

// ---------- ConstExpr ----------

class ConstExpr : public Expr {
 public:
  /* implicit */ ConstExpr(int64_t value) :
      Expr{ExprKind::Const},
      value_{value} {}

  int64_t as_int() const {
    return value_;
  }

 private:
  int64_t value_;
};

// ---------- BinaryExpr ----------

class BinaryExpr : public Expr {
 public:
  BinaryExpr(ExprKind kind, Expr* lhs, Expr* rhs) :
      Expr{kind},
      lhs_{lhs},
      rhs_{rhs} {
    assert(lhs && rhs);
  }

  Expr* lhs() const {
    return lhs_;
  }
  Expr* rhs() const {
    return rhs_;
  }

 private:
  Expr* lhs_;
  Expr* rhs_;
};

// ---------- UnaryExpr ----------

class UnaryExpr : public Expr {
 public:
  UnaryExpr(ExprKind kind, Expr* operand) :
      Expr{kind},
      operand_{operand} {
    assert(operand);
  }

  Expr* operand() const {
    return operand_;
  }

 private:
  Expr* operand_;
};

// ---------- ImpliesExpr ----------

class ImpliesExpr : public Expr {
 public:
  ImpliesExpr(Expr* cond, Expr* body) :
      Expr{ExprKind::Implies},
      cond_{cond},
      body_{body} {
    assert(cond && body);
  }

  Expr* cond() const {
    return cond_;
  }
  Expr* body() const {
    return body_;
  }

 private:
  Expr* cond_;
  Expr* body_;
};

// ---------- IteExpr (if-then-else for array min/max folding; Z3 ite) ----------

class IteExpr : public Expr {
 public:
  IteExpr(Expr* cond, Expr* then_expr, Expr* else_expr) :
      Expr{ExprKind::Ite},
      cond_{cond},
      then_{then_expr},
      else_{else_expr} {
    assert(cond && then_expr && else_expr);
  }

  Expr* cond() const {
    return cond_;
  }
  Expr* then_expr() const {
    return then_;
  }
  Expr* else_expr() const {
    return else_;
  }

 private:
  Expr* cond_;
  Expr* then_;
  Expr* else_;
};

// ---------- CallExpr ----------

class CallExpr : public Expr {
 public:
  using EvalFn = std::function<int64_t(const std::vector<int64_t>&)>;

  CallExpr(std::vector<Expr*> args, EvalFn eval_fn) :
      Expr{ExprKind::Call},
      args_{std::move(args)},
      eval_fn_{std::move(eval_fn)} {
    assert(eval_fn_);
    for (auto* arg : args_) {
      assert(arg);
    }
  }

  const std::vector<Expr*>& args() const {
    return args_;
  }

  int64_t invoke(const std::vector<int64_t>& concrete_args) const {
    return eval_fn_(concrete_args);
  }

 private:
  std::vector<Expr*> args_;
  EvalFn eval_fn_;
};

// ---------- Array-related forward declarations ----------

class ArrayExprBase;
class MapExprBase;

class ArraySizeExpr : public Expr {
 public:
  explicit ArraySizeExpr(ArrayExprBase* array) :
      Expr{ExprKind::ArraySize},
      array_{array} {
    assert(array);
  }

  ArrayExprBase* array() const {
    return array_;
  }

 private:
  ArrayExprBase* array_;
};

class ArrayElemExpr : public Expr {
 public:
  ArrayElemExpr(ArrayExprBase* array, Expr* index) :
      Expr{ExprKind::ArrayElem},
      array_{array},
      index_{index} {
    assert(array && index);
  }

  ArrayExprBase* array() const {
    return array_;
  }
  Expr* index() const {
    return index_;
  }

 private:
  ArrayExprBase* array_;
  Expr* index_;
};

// ---------- ForEachIExpr ----------

class ForEachIExpr : public Expr {
 public:
  ForEachIExpr(ArrayExprBase* array, Expr* sym_idx, Expr* sym_elem, std::vector<Expr*> body) :
      Expr{ExprKind::ForEachI},
      array_{array},
      sym_idx_{sym_idx},
      sym_elem_{sym_elem},
      body_{std::move(body)} {
    assert(array && sym_idx && sym_elem);
  }

  ArrayExprBase* array() const {
    return array_;
  }
  Expr* sym_idx() const {
    return sym_idx_;
  }
  Expr* sym_elem() const {
    return sym_elem_;
  }
  const std::vector<Expr*>& body() const {
    return body_;
  }

 private:
  ArrayExprBase* array_;
  Expr* sym_idx_;
  Expr* sym_elem_;
  std::vector<Expr*> body_;
};

// ---------- ForEachKVExpr (map entries) ----------

class ForEachKVExpr : public Expr {
 public:
  ForEachKVExpr(MapExprBase* map, Expr* sym_idx, Expr* sym_key, Expr* sym_value,
                std::vector<Expr*> body) :
      Expr{ExprKind::ForEachKV},
      map_{map},
      sym_idx_{sym_idx},
      sym_key_{sym_key},
      sym_value_{sym_value},
      body_{std::move(body)} {
    assert(map && sym_idx && sym_key && sym_value);
  }

  MapExprBase* map() const {
    return map_;
  }
  Expr* sym_idx() const {
    return sym_idx_;
  }
  Expr* sym_key() const {
    return sym_key_;
  }
  Expr* sym_value() const {
    return sym_value_;
  }
  const std::vector<Expr*>& body() const {
    return body_;
  }

 private:
  MapExprBase* map_;
  Expr* sym_idx_;
  Expr* sym_key_;
  Expr* sym_value_;
  std::vector<Expr*> body_;
};

// ---------- UniqueExpr ----------

class UniqueExpr : public Expr {
 public:
  explicit UniqueExpr(ArrayExprBase* array) :
      Expr{ExprKind::Unique},
      array_{array} {
    assert(array);
  }

  ArrayExprBase* array() const {
    return array_;
  }

 private:
  ArrayExprBase* array_;
};

// ---------- ArrayAggExpr (SV array.sum / min / max style) ----------

enum class ArrayAggKind { Sum, Product, And, Or, Xor, Min, Max };

class ArrayAggExpr : public Expr {
 public:
  ArrayAggExpr(ArrayAggKind agg, ArrayExprBase* array) :
      Expr{ExprKind::ArrayAgg},
      agg_{agg},
      array_{array},
      sym_idx_{nullptr},
      sym_elem_{nullptr},
      value_expr_{nullptr} {
    assert(array);
  }

  ArrayAggExpr(ArrayAggKind agg, ArrayExprBase* array, Expr* sym_idx, Expr* sym_elem,
               Expr* value_expr) :
      Expr{ExprKind::ArrayAgg},
      agg_{agg},
      array_{array},
      sym_idx_{sym_idx},
      sym_elem_{sym_elem},
      value_expr_{value_expr} {
    assert(array && sym_idx && sym_elem && value_expr);
  }

  ArrayAggKind agg_kind() const {
    return agg_;
  }

  ArrayExprBase* array() const {
    return array_;
  }

  bool has_with() const {
    return value_expr_ != nullptr;
  }

  Expr* sym_idx() const {
    return sym_idx_;
  }

  Expr* sym_elem() const {
    return sym_elem_;
  }

  Expr* value_expr() const {
    return value_expr_;
  }

 private:
  ArrayAggKind agg_;
  ArrayExprBase* array_;
  Expr* sym_idx_;
  Expr* sym_elem_;
  Expr* value_expr_;
};

// ---------- Operator overloads ----------
// All return Expr& — caller does NOT own the memory.
// Memory is managed by ConstraintCollector's arena.

// We use a thread-local arena pointer for operator-created nodes.
namespace detail {
struct Arena {
  std::vector<std::unique_ptr<Expr>> nodes;
};

void set_arena(Arena* arena);
Arena* get_arena();

Expr& make_binary(ExprKind kind, Expr& lhs, Expr& rhs);
Expr& make_unary(ExprKind kind, Expr& operand);
Expr& make_const(int64_t value);
Expr& make_ite(Expr& cond, Expr& then_expr, Expr& else_expr);
Expr& make_call(std::vector<Expr*> args, CallExpr::EvalFn eval_fn);
}  // namespace detail

// Expr & Expr
Expr& operator+(Expr& lhs, Expr& rhs);
Expr& operator-(Expr& lhs, Expr& rhs);
Expr& operator*(Expr& lhs, Expr& rhs);
Expr& operator/(Expr& lhs, Expr& rhs);
Expr& operator%(Expr& lhs, Expr& rhs);
Expr& operator<(Expr& lhs, Expr& rhs);
Expr& operator<=(Expr& lhs, Expr& rhs);
Expr& operator>(Expr& lhs, Expr& rhs);
Expr& operator>=(Expr& lhs, Expr& rhs);
Expr& operator==(Expr& lhs, Expr& rhs);
Expr& operator!=(Expr& lhs, Expr& rhs);
Expr& operator&&(Expr& lhs, Expr& rhs);
Expr& operator||(Expr& lhs, Expr& rhs);

// int & Expr  /  Expr & int  (auto-wrap ConstExpr)
Expr& operator+(int64_t lhs, Expr& rhs);
Expr& operator+(Expr& lhs, int64_t rhs);
Expr& operator-(int64_t lhs, Expr& rhs);
Expr& operator-(Expr& lhs, int64_t rhs);
Expr& operator*(int64_t lhs, Expr& rhs);
Expr& operator*(Expr& lhs, int64_t rhs);
Expr& operator/(int64_t lhs, Expr& rhs);
Expr& operator/(Expr& lhs, int64_t rhs);
Expr& operator%(int64_t lhs, Expr& rhs);
Expr& operator%(Expr& lhs, int64_t rhs);
Expr& operator<(int64_t lhs, Expr& rhs);
Expr& operator<(Expr& lhs, int64_t rhs);
Expr& operator<=(int64_t lhs, Expr& rhs);
Expr& operator<=(Expr& lhs, int64_t rhs);
Expr& operator>(int64_t lhs, Expr& rhs);
Expr& operator>(Expr& lhs, int64_t rhs);
Expr& operator>=(int64_t lhs, Expr& rhs);
Expr& operator>=(Expr& lhs, int64_t rhs);
Expr& operator==(int64_t lhs, Expr& rhs);
Expr& operator==(Expr& lhs, int64_t rhs);
Expr& operator!=(int64_t lhs, Expr& rhs);
Expr& operator!=(Expr& lhs, int64_t rhs);

// Unary
Expr& operator!(Expr& operand);

// Free functions
Expr& abs(Expr& operand);

}  // namespace fuzzy

#endif  // FUZZY_AST_EXPR_H
