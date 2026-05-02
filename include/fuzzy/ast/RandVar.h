#ifndef FUZZY_AST_RAND_VAR_H
#define FUZZY_AST_RAND_VAR_H

#include <concepts>
#include <cstddef>
#include <string>

#include "fuzzy/ast/Expr.h"

namespace fuzzy {

// Base class for type-erased variable access (used by solver)
class VarExprBase : public Expr {
 public:
  VarExprBase(const std::string& name) :
      Expr{ExprKind::Var},
      name_{name} {}

  const std::string& name() const {
    return name_;
  }

  // Type-erased write-back from int64_t (Z3 result)
  virtual void write_back(int64_t value) = 0;

  // Type-erased read of the current value as int64_t
  virtual int64_t read_as_int64() const = 0;

  // Whether this is a symbolic variable (not bound to real output)
  virtual bool is_symbolic() const = 0;

 private:
  std::string name_;
};

// Concrete typed variable expression
template <std::integral T>
class VarExpr : public VarExprBase {
 public:
  // Bound variable: writes back to out
  VarExpr(T& out, const std::string& name) :
      VarExprBase{name},
      out_{&out},
      symbolic_{false} {}

  // Symbolic variable: used by for_each_i, not bound to real output
  explicit VarExpr(const std::string& name) :
      VarExprBase{name},
      out_{&dummy_},
      symbolic_{true} {}

  T& out() {
    return *out_;
  }
  const T& out() const {
    return *out_;
  }

  void write_back(int64_t value) override {
    *out_ = static_cast<T>(value);
  }

  int64_t read_as_int64() const override {
    return static_cast<int64_t>(*out_);
  }

  bool is_symbolic() const override {
    return symbolic_;
  }

 private:
  T* out_;
  bool symbolic_;
  T dummy_{};
};

}  // namespace fuzzy

#endif  // FUZZY_AST_RAND_VAR_H
