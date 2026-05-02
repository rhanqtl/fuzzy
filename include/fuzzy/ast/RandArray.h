#ifndef FUZZY_AST_RAND_ARRAY_H
#define FUZZY_AST_RAND_ARRAY_H

#include <cassert>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "fuzzy/ast/Expr.h"
#include "fuzzy/ast/RandVar.h"

namespace fuzzy {

// Abstract base for type-erased array access (used by solver)
class ArrayExprBase : public Expr {
 public:
  ArrayExprBase() :
      Expr{ExprKind::Var} {}  // kind is Var but subclass is array

  virtual ~ArrayExprBase() = default;

  // Get the size proxy expression
  virtual ArraySizeExpr& size() = 0;

  // Symbolic element for for_each_i
  virtual VarExprBase& symbolic_element() = 0;

  // Subscript with symbolic index
  virtual ArrayElemExpr& operator[](Expr& idx) = 0;

  // Type-erased resize
  virtual void resize(std::size_t n) = 0;

  // Get number of materialized element variables
  virtual std::size_t num_elem_vars() const = 0;

  // Get materialized element variable by index
  virtual VarExprBase& elem_var(std::size_t i) = 0;

  // Create element variables after size is known
  virtual void materialize_elements(std::size_t n) = 0;

  // Get the concrete size (after solve phase 1 wrote back size_var)
  virtual std::size_t concrete_size() const = 0;

  // Get the size variable (type-erased)
  virtual VarExprBase& size_var_base() = 0;

  // Get the actual underlying vector size (not from size_var, but from the real container)
  virtual std::size_t actual_size() const = 0;
};

// Concrete typed array expression
template <std::integral T>
class ArrayExpr : public ArrayExprBase {
 public:
  ArrayExpr(std::vector<T>& out, const std::string& name) :
      out_{out},
      name_{name},
      size_var_{size_dummy_, name + ".size"},
      size_expr_{this} {}

  ArraySizeExpr& size() override {
    return size_expr_;
  }

  VarExprBase& symbolic_element() override {
    // Always create a fresh symbolic element (needed for nested for_each_i)
    sym_elems_.push_back(
        std::make_unique<VarExpr<T>>(name_ + ".__elem" + std::to_string(sym_elems_.size())));
    return *sym_elems_.back();
  }

  ArrayElemExpr& operator[](Expr& idx) override {
    // Allocate a new ArrayElemExpr each time (managed by arena externally)
    auto* elem = new ArrayElemExpr(this, &idx);
    owned_elems_.emplace_back(elem);
    return *elem;
  }

  void resize(std::size_t n) override {
    out_.resize(n);
  }

  std::size_t num_elem_vars() const override {
    return elem_vars_.size();
  }

  VarExprBase& elem_var(std::size_t i) override {
    assert(i < elem_vars_.size());
    return *elem_vars_[i];
  }

  void materialize_elements(std::size_t n) override {
    elem_vars_.clear();
    elem_vars_.reserve(n);
    for (std::size_t i = 0; i < n; i++) {
      elem_vars_.push_back(
          std::make_unique<VarExpr<T>>(out_[i], name_ + "[" + std::to_string(i) + "]"));
    }
  }

  std::size_t concrete_size() const override {
    return size_dummy_;
  }

  VarExprBase& size_var_base() override {
    return size_var_;
  }

  std::size_t actual_size() const override {
    return out_.size();
  }

  VarExpr<std::size_t>& size_var() {
    return size_var_;
  }
  const std::string& name() const {
    return name_;
  }
  std::vector<T>& out() {
    return out_;
  }

 private:
  std::vector<T>& out_;
  std::string name_;

  // Size tracking
  std::size_t size_dummy_{0};
  VarExpr<std::size_t> size_var_;
  ArraySizeExpr size_expr_;

  // Symbolic elements for for_each_i (one per nesting level)
  std::vector<std::unique_ptr<VarExpr<T>>> sym_elems_;

  // Materialized per-element variables (created after size is known)
  std::vector<std::unique_ptr<VarExpr<T>>> elem_vars_;

  // ArrayElemExpr nodes created by operator[]
  std::vector<std::unique_ptr<ArrayElemExpr>> owned_elems_;
};

}  // namespace fuzzy

#endif  // FUZZY_AST_RAND_ARRAY_H
