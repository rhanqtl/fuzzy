#ifndef FUZZY_CONSTRAINT_COLLECTOR_H
#define FUZZY_CONSTRAINT_COLLECTOR_H

#include <cassert>
#include <concepts>
#include <cstddef>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

#include "fuzzy/ast/Expr.h"
#include "fuzzy/ast/RandArray.h"
#include "fuzzy/ast/RandVar.h"

namespace fuzzy {

class ConstraintCollector {
 public:
  struct Block {
    std::string name;
    std::vector<Expr*> constraints;
  };

  ConstraintCollector() {
    // Initialize with a default scope
    scope_stack_.emplace_back();
    // Install arena so operators can allocate
    detail::set_arena(&arena_);
  }

  ~ConstraintCollector() {
    // Clear arena pointer
    if (detail::get_arena() == &arena_) {
      detail::set_arena(nullptr);
    }
  }

  ConstraintCollector(const ConstraintCollector&) = delete;
  ConstraintCollector& operator=(const ConstraintCollector&) = delete;

  // ---- Proxy creation (overload dispatch) ----

  template <std::integral T>
  VarExpr<T>& create_proxy(T& out, const std::string& name = "") {
    auto ptr = std::make_unique<VarExpr<T>>(out, name.empty() ? auto_name() : name);
    VarExpr<T>& ref = *ptr;
    all_vars_.push_back(ptr.get());
    arena_.nodes.push_back(std::move(ptr));
    return ref;
  }

  template <std::integral T>
  ArrayExpr<T>& create_proxy(std::vector<T>& out, const std::string& name = "") {
    auto ptr = std::make_unique<ArrayExpr<T>>(out, name.empty() ? auto_name() : name);
    ArrayExpr<T>& ref = *ptr;
    all_arrays_.push_back(ptr.get());
    // Also register the size variable for phase-1 solving
    all_vars_.push_back(&ref.size_var());
    arena_.nodes.push_back(std::move(ptr));
    return ref;
  }

  // Create unbound symbolic variable (for for_each_i)
  template <std::integral T>
  VarExpr<T>& create_symbolic(const std::string& name) {
    auto ptr = std::make_unique<VarExpr<T>>(name);
    VarExpr<T>& ref = *ptr;
    arena_.nodes.push_back(std::move(ptr));
    return ref;
  }

  // ---- Constraint collection ----

  void add_constraint(Expr& e) {
    assert(!scope_stack_.empty());
    scope_stack_.back().push_back(&e);
  }

  // Scope stack (for for_each_i nesting)
  void push_scope() {
    scope_stack_.emplace_back();
  }

  std::vector<Expr*> pop_scope() {
    assert(scope_stack_.size() > 1 && "Cannot pop the root scope");
    auto top = std::move(scope_stack_.back());
    scope_stack_.pop_back();
    return top;
  }

  // ---- Block management ----

  void begin_block(const char* name) {
    // Flush current scope constraints into the previous block if any
    if (!blocks_.empty()) {
      auto& cur = scope_stack_.back();
      for (auto* c : cur) {
        blocks_.back().constraints.push_back(c);
      }
      cur.clear();
    }
    blocks_.push_back(Block{name, {}});
  }

  // Flush remaining constraints into the last block
  void finalize() {
    if (blocks_.empty()) {
      blocks_.push_back(Block{"default", {}});
    }
    auto& cur = scope_stack_.back();
    for (auto* c : cur) {
      blocks_.back().constraints.push_back(c);
    }
    cur.clear();
  }

  // ---- Higher-order constraints ----

  template <typename F>
  void for_each_i(ArrayExprBase& arr, F&& body) {
    // 1. Create symbolic variables
    auto& sym_idx = create_symbolic<std::size_t>("__idx_" + std::to_string(foreach_counter_++));
    auto& sym_elem = arr.symbolic_element();

    // 2. Push sub-scope, execute lambda symbolically
    push_scope();
    body(sym_idx, sym_elem);
    auto body_constraints = pop_scope();

    // 3. Wrap as ForEachI AST node, add to outer scope
    auto ptr = std::make_unique<ForEachIExpr>(&arr, &sym_idx, &sym_elem,
                                              std::move(body_constraints));
    ForEachIExpr& ref = *ptr;
    arena_.nodes.push_back(std::move(ptr));
    add_constraint(ref);
  }

  void add_unique(ArrayExprBase& arr) {
    auto ptr = std::make_unique<UniqueExpr>(&arr);
    UniqueExpr& ref = *ptr;
    arena_.nodes.push_back(std::move(ptr));
    add_constraint(ref);
  }

  Expr& make_implies(Expr& cond, Expr& body) {
    auto ptr = std::make_unique<ImpliesExpr>(&cond, &body);
    ImpliesExpr& ref = *ptr;
    arena_.nodes.push_back(std::move(ptr));
    return ref;
  }

  // ---- Result access ----

  const std::vector<Block>& blocks() const {
    return blocks_;
  }
  std::vector<Block>& blocks() {
    return blocks_;
  }

  // Access all registered variable/array proxies
  const std::vector<VarExprBase*>& all_vars() const {
    return all_vars_;
  }
  const std::vector<ArrayExprBase*>& all_arrays() const {
    return all_arrays_;
  }

  // ---- Arena access ----

  detail::Arena& arena() {
    return arena_;
  }

 private:
  std::string auto_name() {
    return "__v" + std::to_string(name_counter_++);
  }

  std::vector<Block> blocks_;
  std::vector<std::vector<Expr*>> scope_stack_;
  detail::Arena arena_;

  std::vector<VarExprBase*> all_vars_;
  std::vector<ArrayExprBase*> all_arrays_;

  int name_counter_{0};
  int foreach_counter_{0};
};

}  // namespace fuzzy

#endif  // FUZZY_CONSTRAINT_COLLECTOR_H
