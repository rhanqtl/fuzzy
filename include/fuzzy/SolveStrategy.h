#ifndef FUZZY_SOLVE_STRATEGY_H
#define FUZZY_SOLVE_STRATEGY_H

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "fuzzy/ConstraintCollector.h"
#include "fuzzy/ast/Expr.h"
#include "fuzzy/ast/RandArray.h"
#include "fuzzy/ast/RandVar.h"
#include "fuzzy/solver/Z3Solver.h"

namespace fuzzy {

// Represents a node in the dependency graph
enum class SolveNodeKind { ScalarVar, ArraySize, ArrayElems };

struct SolveNode {
  SolveNodeKind kind;
  Expr* expr;            // VarExprBase* for ScalarVar, ArrayExprBase* for ArraySize/ArrayElems
  ArrayExprBase* array;  // non-null for ArraySize/ArrayElems
  bool solved = false;
};

class SolveStrategy {
 public:
  bool solve(ConstraintCollector& cc, unsigned seed = 0) {
    // Collect all blocks' constraints
    auto& blocks = cc.blocks();

    // Build all constraints into a flat list
    std::vector<Expr*> all_constraints;
    for (auto& block : blocks) {
      for (auto* c : block.constraints) {
        all_constraints.push_back(c);
      }
    }

    // Phase 1: Separate constraints by array-element involvement first.
    std::vector<Expr*> scalar_constraints_all;
    std::vector<Expr*> element_constraints;

    for (auto* c : all_constraints) {
      if (involves_array_elements(c)) {
        element_constraints.push_back(c);
      } else {
        scalar_constraints_all.push_back(c);
      }
    }

    // Split scalar constraints into:
    //  - no-call constraints: directly translatable to Z3
    //  - call constraints: reduced to constants after argument solve
    std::vector<Expr*> scalar_constraints_no_call;
    std::vector<Expr*> scalar_constraints_with_call;
    for (auto* c : scalar_constraints_all) {
      if (contains_call(c)) {
        scalar_constraints_with_call.push_back(c);
      } else {
        scalar_constraints_no_call.push_back(c);
      }
    }

    // Phase 1A: Solve call arguments (and other scalar vars) without call constraints.
    {
      Z3Solver solver(seed);

      // Register scalar vars
      for (auto* var : cc.all_vars()) {
        solver.get_or_create_var(*var);
      }
      // Register array size vars
      for (auto* arr : cc.all_arrays()) {
        solver.get_or_create_var(arr->size_var_base());
      }

      // Add scalar constraints without function calls.
      for (auto* c : scalar_constraints_no_call) {
        solver.add_constraint(*c);
      }

      if (!solver.solve())
        return false;
      solver.write_back();
    }

    // Phase 1B: Reduce function calls to constants, then solve remaining scalar constraints.
    if (!scalar_constraints_with_call.empty()) {
      std::vector<Expr*> reduced_call_constraints;
      reduced_call_constraints.reserve(scalar_constraints_with_call.size());
      for (auto* c : scalar_constraints_with_call) {
        auto* reduced = reduce_calls(c, cc);
        if (!reduced || contains_call(reduced)) {
          return false;
        }
        reduced_call_constraints.push_back(reduced);
      }

      Z3Solver solver(seed + 7);
      for (auto* var : cc.all_vars()) {
        solver.get_or_create_var(*var);
      }
      for (auto* arr : cc.all_arrays()) {
        solver.get_or_create_var(arr->size_var_base());
      }

      // Keep no-call constraints active so non-argument vars remain properly constrained.
      for (auto* c : scalar_constraints_no_call) {
        solver.add_constraint(*c);
      }
      for (auto* c : reduced_call_constraints) {
        solver.add_constraint(*c);
      }

      std::unordered_set<Expr*> pinned_vars;
      for (auto* c : scalar_constraints_with_call) {
        collect_call_argument_vars(c, pinned_vars);
      }
      for (auto* v : pinned_vars) {
        pin_solved_expr(solver, *v);
      }

      if (!solver.solve())
        return false;
      solver.write_back();
    }

    // Materialize array elements based on solved sizes
    for (auto* arr : cc.all_arrays()) {
      std::size_t concrete_size = arr->concrete_size();
      arr->resize(concrete_size);
      arr->materialize_elements(concrete_size);
    }

    // Phase 2: Expand higher-order constraints and solve array elements
    {
      Z3Solver solver(seed + 1);

      // Register array element vars
      for (auto* arr : cc.all_arrays()) {
        for (std::size_t i = 0; i < arr->num_elem_vars(); i++) {
          solver.get_or_create_var(arr->elem_var(i));
        }
      }

      // Re-register scalar vars (they'll be used in expanded constraints)
      for (auto* var : cc.all_vars()) {
        solver.get_or_create_var(*var);
      }

      // Pin scalar vars to their phase-1 solved values
      for (auto* var : cc.all_vars()) {
        pin_solved_expr(solver, *var);
      }

      // Expand and add element constraints
      for (auto* c : element_constraints) {
        auto expanded = expand_constraint(c, cc);
        for (auto* ec : expanded) {
          // Recursively expand nested ForEachI
          if (ec->kind() == ExprKind::ForEachI) {
            auto inner = expand_constraint(ec, cc);
            for (auto* ic : inner) {
              solver.add_constraint(*ic);
            }
          } else {
            solver.add_constraint(*ec);
          }
        }
      }

      if (!solver.solve())
        return false;
      solver.write_back();
    }

    return true;
  }

 private:
  // Check if an expression involves array elements
  bool involves_array_elements(Expr* e) {
    if (!e)
      return false;
    switch (e->kind()) {
      case ExprKind::ForEachI:
      case ExprKind::Unique:
      case ExprKind::ArrayElem:
        return true;
      case ExprKind::Add:
      case ExprKind::Sub:
      case ExprKind::Less:
      case ExprKind::LessEqual:
      case ExprKind::Greater:
      case ExprKind::GreaterEqual:
      case ExprKind::Equal:
      case ExprKind::NotEqual:
      case ExprKind::LogicalAnd:
      case ExprKind::LogicalOr: {
        auto& b = static_cast<BinaryExpr&>(*e);
        return involves_array_elements(b.lhs()) || involves_array_elements(b.rhs());
      }
      case ExprKind::Abs:
      case ExprKind::LogicalNot: {
        auto& u = static_cast<UnaryExpr&>(*e);
        return involves_array_elements(u.operand());
      }
      case ExprKind::Implies: {
        auto& imp = static_cast<ImpliesExpr&>(*e);
        return involves_array_elements(imp.cond()) || involves_array_elements(imp.body());
      }
      case ExprKind::Call: {
        auto& call = static_cast<CallExpr&>(*e);
        for (auto* arg : call.args()) {
          if (involves_array_elements(arg)) {
            return true;
          }
        }
        return false;
      }
      default:
        return false;
    }
  }

  // Expand a higher-order constraint into concrete constraints
  std::vector<Expr*> expand_constraint(Expr* e, ConstraintCollector& cc) {
    std::vector<Expr*> result;

    if (e->kind() == ExprKind::ForEachI) {
      auto& forEach = static_cast<ForEachIExpr&>(*e);
      auto* arr = forEach.array();
      std::size_t n = arr->num_elem_vars();

      for (std::size_t i = 0; i < n; i++) {
        auto& elem_i = arr->elem_var(i);

        // For each body constraint, substitute sym_idx with i, sym_elem with elem_i
        for (auto* body_expr : forEach.body()) {
          auto* expanded = substitute(body_expr, forEach.sym_idx(), forEach.sym_elem(), i, &elem_i,
                                      cc);
          if (expanded) {
            result.push_back(expanded);
          }
        }
      }
    } else if (e->kind() == ExprKind::Unique) {
      auto& uniq = static_cast<UniqueExpr&>(*e);
      auto* arr = uniq.array();
      std::size_t n = arr->num_elem_vars();

      // Generate pairwise != constraints
      for (std::size_t i = 0; i < n; i++) {
        for (std::size_t j = i + 1; j < n; j++) {
          auto& ne = detail::make_binary(ExprKind::NotEqual, arr->elem_var(i), arr->elem_var(j));
          result.push_back(&ne);
        }
      }
    } else {
      // Non-expandable constraint, pass through
      result.push_back(e);
    }

    return result;
  }

  // Substitute symbolic variables in an expression tree with concrete values
  Expr* substitute(Expr* e, Expr* sym_idx, Expr* sym_elem, std::size_t concrete_idx,
                   VarExprBase* concrete_elem, ConstraintCollector& cc) {
    if (!e)
      return nullptr;

    // Check if this is the symbolic index
    if (e == sym_idx) {
      return &detail::make_const(static_cast<int64_t>(concrete_idx));
    }

    // Check if this is the symbolic element
    if (e == sym_elem) {
      return concrete_elem;
    }

    switch (e->kind()) {
      case ExprKind::Var:
      case ExprKind::Const:
        return e;  // Leaf nodes, no substitution needed

      case ExprKind::Add:
      case ExprKind::Sub:
      case ExprKind::Less:
      case ExprKind::LessEqual:
      case ExprKind::Greater:
      case ExprKind::GreaterEqual:
      case ExprKind::Equal:
      case ExprKind::NotEqual:
      case ExprKind::LogicalAnd:
      case ExprKind::LogicalOr: {
        auto& b = static_cast<BinaryExpr&>(*e);
        auto* new_lhs = substitute(b.lhs(), sym_idx, sym_elem, concrete_idx, concrete_elem, cc);
        auto* new_rhs = substitute(b.rhs(), sym_idx, sym_elem, concrete_idx, concrete_elem, cc);
        if (new_lhs == b.lhs() && new_rhs == b.rhs())
          return e;
        return &detail::make_binary(e->kind(), *new_lhs, *new_rhs);
      }

      case ExprKind::Abs:
      case ExprKind::LogicalNot: {
        auto& u = static_cast<UnaryExpr&>(*e);
        auto* new_op = substitute(u.operand(), sym_idx, sym_elem, concrete_idx, concrete_elem, cc);
        if (new_op == u.operand())
          return e;
        return &detail::make_unary(e->kind(), *new_op);
      }

      case ExprKind::Implies: {
        auto& imp = static_cast<ImpliesExpr&>(*e);
        auto* new_cond = substitute(imp.cond(), sym_idx, sym_elem, concrete_idx, concrete_elem, cc);
        auto* new_body = substitute(imp.body(), sym_idx, sym_elem, concrete_idx, concrete_elem, cc);
        if (new_cond == imp.cond() && new_body == imp.body())
          return e;
        return &cc.make_implies(*new_cond, *new_body);
      }

      case ExprKind::ForEachI: {
        // Nested ForEachI — expand recursively
        auto& inner = static_cast<ForEachIExpr&>(*e);
        // First substitute in the body
        std::vector<Expr*> new_body;
        for (auto* body_expr : inner.body()) {
          new_body.push_back(
              substitute(body_expr, sym_idx, sym_elem, concrete_idx, concrete_elem, cc));
        }
        // Create a new ForEachIExpr with substituted body
        auto ptr = std::make_unique<ForEachIExpr>(inner.array(), inner.sym_idx(), inner.sym_elem(),
                                                  std::move(new_body));
        auto* result = ptr.get();
        cc.arena().nodes.push_back(std::move(ptr));
        // Now expand this inner ForEachI
        return result;
      }

      case ExprKind::ArrayElem: {
        auto& ae = static_cast<ArrayElemExpr&>(*e);
        auto* new_idx = substitute(ae.index(), sym_idx, sym_elem, concrete_idx, concrete_elem, cc);
        if (new_idx == ae.index())
          return e;

        // If new_idx is a constant, resolve to concrete element variable
        if (new_idx->kind() == ExprKind::Const) {
          auto& c = static_cast<ConstExpr&>(*new_idx);
          std::size_t idx = static_cast<std::size_t>(c.as_int());
          if (idx < ae.array()->num_elem_vars()) {
            return &ae.array()->elem_var(idx);
          }
        }
        // Otherwise create a new ArrayElemExpr
        auto ptr = std::make_unique<ArrayElemExpr>(ae.array(), new_idx);
        auto* result = ptr.get();
        cc.arena().nodes.push_back(std::move(ptr));
        return result;
      }

      case ExprKind::Call: {
        auto& call = static_cast<CallExpr&>(*e);
        bool changed = false;
        std::vector<Expr*> new_args;
        new_args.reserve(call.args().size());
        for (auto* arg : call.args()) {
          auto* new_arg = substitute(arg, sym_idx, sym_elem, concrete_idx, concrete_elem, cc);
          changed = changed || (new_arg != arg);
          new_args.push_back(new_arg);
        }
        if (!changed) {
          return e;
        }
        auto eval_fn = [&call](const std::vector<int64_t>& concrete_args) -> int64_t {
          return call.invoke(concrete_args);
        };
        return &detail::make_call(std::move(new_args), std::move(eval_fn));
      }

      default:
        return e;
    }
  }

  bool contains_call(Expr* e) {
    if (!e) {
      return false;
    }
    switch (e->kind()) {
      case ExprKind::Call:
        return true;
      case ExprKind::Add:
      case ExprKind::Sub:
      case ExprKind::Less:
      case ExprKind::LessEqual:
      case ExprKind::Greater:
      case ExprKind::GreaterEqual:
      case ExprKind::Equal:
      case ExprKind::NotEqual:
      case ExprKind::LogicalAnd:
      case ExprKind::LogicalOr: {
        auto& b = static_cast<BinaryExpr&>(*e);
        return contains_call(b.lhs()) || contains_call(b.rhs());
      }
      case ExprKind::Abs:
      case ExprKind::LogicalNot: {
        auto& u = static_cast<UnaryExpr&>(*e);
        return contains_call(u.operand());
      }
      case ExprKind::Implies: {
        auto& imp = static_cast<ImpliesExpr&>(*e);
        return contains_call(imp.cond()) || contains_call(imp.body());
      }
      case ExprKind::ArrayElem: {
        auto& ae = static_cast<ArrayElemExpr&>(*e);
        return contains_call(ae.index());
      }
      case ExprKind::ForEachI: {
        auto& fe = static_cast<ForEachIExpr&>(*e);
        for (auto* body : fe.body()) {
          if (contains_call(body)) {
            return true;
          }
        }
        return false;
      }
      default:
        return false;
    }
  }

  void collect_vars_for_pin(Expr* e, std::unordered_set<Expr*>& out) {
    if (!e) {
      return;
    }
    switch (e->kind()) {
      case ExprKind::Var:
        out.insert(e);
        return;
      case ExprKind::ArraySize: {
        auto& as = static_cast<ArraySizeExpr&>(*e);
        out.insert(&as.array()->size_var_base());
        return;
      }
      case ExprKind::Add:
      case ExprKind::Sub:
      case ExprKind::Less:
      case ExprKind::LessEqual:
      case ExprKind::Greater:
      case ExprKind::GreaterEqual:
      case ExprKind::Equal:
      case ExprKind::NotEqual:
      case ExprKind::LogicalAnd:
      case ExprKind::LogicalOr: {
        auto& b = static_cast<BinaryExpr&>(*e);
        collect_vars_for_pin(b.lhs(), out);
        collect_vars_for_pin(b.rhs(), out);
        return;
      }
      case ExprKind::Abs:
      case ExprKind::LogicalNot: {
        auto& u = static_cast<UnaryExpr&>(*e);
        collect_vars_for_pin(u.operand(), out);
        return;
      }
      case ExprKind::Implies: {
        auto& imp = static_cast<ImpliesExpr&>(*e);
        collect_vars_for_pin(imp.cond(), out);
        collect_vars_for_pin(imp.body(), out);
        return;
      }
      case ExprKind::ArrayElem: {
        auto& ae = static_cast<ArrayElemExpr&>(*e);
        collect_vars_for_pin(ae.index(), out);
        return;
      }
      case ExprKind::Call: {
        auto& call = static_cast<CallExpr&>(*e);
        for (auto* arg : call.args()) {
          collect_vars_for_pin(arg, out);
        }
        return;
      }
      case ExprKind::ForEachI: {
        auto& fe = static_cast<ForEachIExpr&>(*e);
        for (auto* body : fe.body()) {
          collect_vars_for_pin(body, out);
        }
        return;
      }
      default:
        return;
    }
  }

  void collect_call_argument_vars(Expr* e, std::unordered_set<Expr*>& out) {
    if (!e) {
      return;
    }
    switch (e->kind()) {
      case ExprKind::Call: {
        auto& call = static_cast<CallExpr&>(*e);
        for (auto* arg : call.args()) {
          collect_vars_for_pin(arg, out);
        }
        for (auto* arg : call.args()) {
          collect_call_argument_vars(arg, out);
        }
        return;
      }
      case ExprKind::Add:
      case ExprKind::Sub:
      case ExprKind::Less:
      case ExprKind::LessEqual:
      case ExprKind::Greater:
      case ExprKind::GreaterEqual:
      case ExprKind::Equal:
      case ExprKind::NotEqual:
      case ExprKind::LogicalAnd:
      case ExprKind::LogicalOr: {
        auto& b = static_cast<BinaryExpr&>(*e);
        collect_call_argument_vars(b.lhs(), out);
        collect_call_argument_vars(b.rhs(), out);
        return;
      }
      case ExprKind::Abs:
      case ExprKind::LogicalNot: {
        auto& u = static_cast<UnaryExpr&>(*e);
        collect_call_argument_vars(u.operand(), out);
        return;
      }
      case ExprKind::Implies: {
        auto& imp = static_cast<ImpliesExpr&>(*e);
        collect_call_argument_vars(imp.cond(), out);
        collect_call_argument_vars(imp.body(), out);
        return;
      }
      case ExprKind::ArrayElem: {
        auto& ae = static_cast<ArrayElemExpr&>(*e);
        collect_call_argument_vars(ae.index(), out);
        return;
      }
      case ExprKind::ForEachI: {
        auto& fe = static_cast<ForEachIExpr&>(*e);
        for (auto* body : fe.body()) {
          collect_call_argument_vars(body, out);
        }
        return;
      }
      default:
        return;
    }
  }

  bool try_eval_scalar_expr(Expr* e, int64_t& out) {
    if (!e) {
      return false;
    }
    switch (e->kind()) {
      case ExprKind::Var:
        out = static_cast<VarExprBase*>(e)->read_as_int64();
        return true;
      case ExprKind::Const:
        out = static_cast<ConstExpr*>(e)->as_int();
        return true;
      case ExprKind::ArraySize: {
        auto& as = static_cast<ArraySizeExpr&>(*e);
        out = static_cast<int64_t>(as.array()->concrete_size());
        return true;
      }
      case ExprKind::Add:
      case ExprKind::Sub:
      case ExprKind::Less:
      case ExprKind::LessEqual:
      case ExprKind::Greater:
      case ExprKind::GreaterEqual:
      case ExprKind::Equal:
      case ExprKind::NotEqual:
      case ExprKind::LogicalAnd:
      case ExprKind::LogicalOr: {
        auto& b = static_cast<BinaryExpr&>(*e);
        int64_t lhs = 0;
        int64_t rhs = 0;
        if (!try_eval_scalar_expr(b.lhs(), lhs) || !try_eval_scalar_expr(b.rhs(), rhs)) {
          return false;
        }
        switch (e->kind()) {
          case ExprKind::Add:
            out = lhs + rhs;
            return true;
          case ExprKind::Sub:
            out = lhs - rhs;
            return true;
          case ExprKind::Less:
            out = lhs < rhs;
            return true;
          case ExprKind::LessEqual:
            out = lhs <= rhs;
            return true;
          case ExprKind::Greater:
            out = lhs > rhs;
            return true;
          case ExprKind::GreaterEqual:
            out = lhs >= rhs;
            return true;
          case ExprKind::Equal:
            out = lhs == rhs;
            return true;
          case ExprKind::NotEqual:
            out = lhs != rhs;
            return true;
          case ExprKind::LogicalAnd:
            out = (lhs != 0) && (rhs != 0);
            return true;
          case ExprKind::LogicalOr:
            out = (lhs != 0) || (rhs != 0);
            return true;
          default:
            return false;
        }
      }
      case ExprKind::Abs:
      case ExprKind::LogicalNot: {
        auto& u = static_cast<UnaryExpr&>(*e);
        int64_t operand = 0;
        if (!try_eval_scalar_expr(u.operand(), operand)) {
          return false;
        }
        if (e->kind() == ExprKind::Abs) {
          out = operand >= 0 ? operand : -operand;
        } else {
          out = operand == 0;
        }
        return true;
      }
      case ExprKind::Implies: {
        auto& imp = static_cast<ImpliesExpr&>(*e);
        int64_t cond = 0;
        int64_t body = 0;
        if (!try_eval_scalar_expr(imp.cond(), cond) || !try_eval_scalar_expr(imp.body(), body)) {
          return false;
        }
        out = (!(cond != 0)) || (body != 0);
        return true;
      }
      case ExprKind::Call: {
        auto& call = static_cast<CallExpr&>(*e);
        std::vector<int64_t> args;
        args.reserve(call.args().size());
        for (auto* arg : call.args()) {
          int64_t v = 0;
          if (!try_eval_scalar_expr(arg, v)) {
            return false;
          }
          args.push_back(v);
        }
        out = call.invoke(args);
        return true;
      }
      default:
        return false;
    }
  }

  Expr* reduce_calls(Expr* e, ConstraintCollector& cc) {
    if (!e) {
      return nullptr;
    }
    switch (e->kind()) {
      case ExprKind::Call: {
        auto& call = static_cast<CallExpr&>(*e);
        std::vector<Expr*> reduced_args;
        reduced_args.reserve(call.args().size());
        bool changed = false;
        for (auto* arg : call.args()) {
          auto* reduced = reduce_calls(arg, cc);
          changed = changed || (reduced != arg);
          reduced_args.push_back(reduced);
        }

        int64_t value = 0;
        bool all_evaluable = true;
        std::vector<int64_t> concrete_args;
        concrete_args.reserve(reduced_args.size());
        for (auto* arg : reduced_args) {
          if (!try_eval_scalar_expr(arg, value)) {
            all_evaluable = false;
            break;
          }
          concrete_args.push_back(value);
        }
        if (all_evaluable) {
          return &detail::make_const(call.invoke(concrete_args));
        }

        if (!changed) {
          return e;
        }
        auto eval_fn = [&call](const std::vector<int64_t>& concrete) -> int64_t {
          return call.invoke(concrete);
        };
        return &detail::make_call(std::move(reduced_args), std::move(eval_fn));
      }
      case ExprKind::Add:
      case ExprKind::Sub:
      case ExprKind::Less:
      case ExprKind::LessEqual:
      case ExprKind::Greater:
      case ExprKind::GreaterEqual:
      case ExprKind::Equal:
      case ExprKind::NotEqual:
      case ExprKind::LogicalAnd:
      case ExprKind::LogicalOr: {
        auto& b = static_cast<BinaryExpr&>(*e);
        auto* lhs = reduce_calls(b.lhs(), cc);
        auto* rhs = reduce_calls(b.rhs(), cc);
        if (lhs == b.lhs() && rhs == b.rhs()) {
          return e;
        }
        return &detail::make_binary(e->kind(), *lhs, *rhs);
      }
      case ExprKind::Abs:
      case ExprKind::LogicalNot: {
        auto& u = static_cast<UnaryExpr&>(*e);
        auto* op = reduce_calls(u.operand(), cc);
        if (op == u.operand()) {
          return e;
        }
        return &detail::make_unary(e->kind(), *op);
      }
      case ExprKind::Implies: {
        auto& imp = static_cast<ImpliesExpr&>(*e);
        auto* cond = reduce_calls(imp.cond(), cc);
        auto* body = reduce_calls(imp.body(), cc);
        if (cond == imp.cond() && body == imp.body()) {
          return e;
        }
        return &cc.make_implies(*cond, *body);
      }
      case ExprKind::ArrayElem: {
        auto& ae = static_cast<ArrayElemExpr&>(*e);
        auto* idx = reduce_calls(ae.index(), cc);
        if (idx == ae.index()) {
          return e;
        }
        if (idx->kind() == ExprKind::Const) {
          auto& c = static_cast<ConstExpr&>(*idx);
          std::size_t i = static_cast<std::size_t>(c.as_int());
          if (i < ae.array()->num_elem_vars()) {
            return &ae.array()->elem_var(i);
          }
        }
        auto ptr = std::make_unique<ArrayElemExpr>(ae.array(), idx);
        auto* result = ptr.get();
        cc.arena().nodes.push_back(std::move(ptr));
        return result;
      }
      case ExprKind::ForEachI: {
        auto& fe = static_cast<ForEachIExpr&>(*e);
        std::vector<Expr*> body;
        body.reserve(fe.body().size());
        bool changed = false;
        for (auto* b : fe.body()) {
          auto* reduced = reduce_calls(b, cc);
          changed = changed || (reduced != b);
          body.push_back(reduced);
        }
        if (!changed) {
          return e;
        }
        auto ptr = std::make_unique<ForEachIExpr>(fe.array(), fe.sym_idx(), fe.sym_elem(),
                                                  std::move(body));
        auto* result = ptr.get();
        cc.arena().nodes.push_back(std::move(ptr));
        return result;
      }
      default:
        return e;
    }
  }

  void pin_solved_expr(Z3Solver& solver, Expr& expr) {
    auto* var = dynamic_cast<VarExprBase*>(&expr);
    if (!var || var->is_symbolic())
      return;
    int64_t val = var->read_as_int64();
    auto z3var = solver.get_or_create_var(*var);
    solver.solver().add(z3var == solver.context().int_val(val));
  }
};

}  // namespace fuzzy

#endif  // FUZZY_SOLVE_STRATEGY_H
