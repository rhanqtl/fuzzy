#ifndef FUZZY_SOLVE_STRATEGY_H
#define FUZZY_SOLVE_STRATEGY_H

#include <algorithm>
#include <cassert>
#include <cstddef>
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
  bool solve(ConstraintCollector& cc) {
    // Collect all blocks' constraints
    auto& blocks = cc.blocks();

    // Build all constraints into a flat list
    std::vector<Expr*> all_constraints;
    for (auto& block : blocks) {
      for (auto* c : block.constraints) {
        all_constraints.push_back(c);
      }
    }

    // Phase 1: Separate constraints into those that only involve scalars/array-sizes
    //          vs those that involve array elements (ForEachI, Unique, etc.)
    std::vector<Expr*> scalar_constraints;
    std::vector<Expr*> element_constraints;

    for (auto* c : all_constraints) {
      if (involves_array_elements(c)) {
        element_constraints.push_back(c);
      } else {
        scalar_constraints.push_back(c);
      }
    }

    // Phase 1: Solve scalar variables and array sizes
    {
      Z3Solver solver;

      // Register scalar vars
      for (auto* var : cc.all_vars()) {
        solver.get_or_create_var(*var);
      }
      // Register array size vars
      for (auto* arr : cc.all_arrays()) {
        solver.get_or_create_var(arr->size_var_base());
      }

      // Add scalar constraints (translate ArraySizeExpr → size_var)
      for (auto* c : scalar_constraints) {
        solver.add_constraint(*c);
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
      Z3Solver solver;

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
        pin_solved_var(solver, *var, cc);
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

      default:
        return e;
    }
  }

  void pin_solved_var(Z3Solver& solver, VarExprBase& var, ConstraintCollector& cc) {
    if (var.is_symbolic())
      return;
    int64_t val = var.read_as_int64();
    auto z3var = solver.get_or_create_var(var);
    solver.solver().add(z3var == solver.context().int_val(val));
  }
};

}  // namespace fuzzy

#endif  // FUZZY_SOLVE_STRATEGY_H
