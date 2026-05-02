#ifndef FUZZY_AST_EXPR_H
#define FUZZY_AST_EXPR_H

#include <cassert>
#include <cstddef>
#include <cstdint>
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

  // Array-related
  ArraySize,
  ArrayElem,

  // Higher-order constraints (expanded before reaching Z3)
  ForEachI,
  Unique,
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

// ---------- Array-related forward declarations ----------

class ArrayExprBase;

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
}  // namespace detail

// Expr & Expr
Expr& operator+(Expr& lhs, Expr& rhs);
Expr& operator-(Expr& lhs, Expr& rhs);
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
