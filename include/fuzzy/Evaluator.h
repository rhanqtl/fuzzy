#ifndef FUZZY_EVALUATOR_H
#define FUZZY_EVALUATOR_H

#include <cmath>
#include <cstdint>
#include <unordered_map>

#include "fuzzy/ConstraintCollector.h"
#include "fuzzy/ast/Expr.h"
#include "fuzzy/ast/RandArray.h"
#include "fuzzy/ast/RandVar.h"

namespace fuzzy {

/// Direct AST evaluator — walks the expression tree using concrete values
/// from bound variables. Used by validate() to check constraints without Z3.
class Evaluator {
 public:
  using Bindings = std::unordered_map<Expr*, int64_t>;

  /// Check all collected constraints against the object's current state.
  bool validate(ConstraintCollector& cc) {
    // Materialize array elements so elem_var(i) is bound to real data
    for (auto* arr : cc.all_arrays()) {
      std::size_t sz = arr->actual_size();
      arr->materialize_elements(sz);
    }

    // Evaluate every constraint in every block
    Bindings empty;
    for (auto& block : cc.blocks()) {
      for (auto* c : block.constraints) {
        if (!eval_bool(c, empty))
          return false;
      }
    }
    return true;
  }

 private:
  /// Evaluate an expression to an int64_t value.
  /// @param bindings  overrides for symbolic variables (from for_each_i expansion)
  int64_t eval(Expr* e, const Bindings& bindings) {
    // Check bindings first (symbolic vars from for_each_i)
    if (auto it = bindings.find(e); it != bindings.end())
      return it->second;

    switch (e->kind()) {
      case ExprKind::Var:
        return static_cast<VarExprBase*>(e)->read_as_int64();

      case ExprKind::Const:
        return static_cast<ConstExpr*>(e)->as_int();

      case ExprKind::Add: {
        auto& b = as<BinaryExpr>(e);
        return eval(b.lhs(), bindings) + eval(b.rhs(), bindings);
      }
      case ExprKind::Sub: {
        auto& b = as<BinaryExpr>(e);
        return eval(b.lhs(), bindings) - eval(b.rhs(), bindings);
      }
      case ExprKind::Abs: {
        auto& u = as<UnaryExpr>(e);
        auto v = eval(u.operand(), bindings);
        return v >= 0 ? v : -v;
      }

      case ExprKind::Less:
        return bool_to_int(eval_cmp<std::less<>>(e, bindings));
      case ExprKind::LessEqual:
        return bool_to_int(eval_cmp<std::less_equal<>>(e, bindings));
      case ExprKind::Greater:
        return bool_to_int(eval_cmp<std::greater<>>(e, bindings));
      case ExprKind::GreaterEqual:
        return bool_to_int(eval_cmp<std::greater_equal<>>(e, bindings));
      case ExprKind::Equal:
        return bool_to_int(eval_cmp<std::equal_to<>>(e, bindings));
      case ExprKind::NotEqual:
        return bool_to_int(eval_cmp<std::not_equal_to<>>(e, bindings));

      case ExprKind::LogicalAnd: {
        auto& b = as<BinaryExpr>(e);
        return bool_to_int(eval(b.lhs(), bindings) && eval(b.rhs(), bindings));
      }
      case ExprKind::LogicalOr: {
        auto& b = as<BinaryExpr>(e);
        return bool_to_int(eval(b.lhs(), bindings) || eval(b.rhs(), bindings));
      }
      case ExprKind::LogicalNot: {
        auto& u = as<UnaryExpr>(e);
        return bool_to_int(!eval(u.operand(), bindings));
      }

      case ExprKind::Implies: {
        auto& imp = as<ImpliesExpr>(e);
        // p → q  ≡  ¬p ∨ q
        return bool_to_int(!eval(imp.cond(), bindings) || eval(imp.body(), bindings));
      }

      case ExprKind::ArraySize: {
        auto& as_expr = static_cast<ArraySizeExpr&>(*e);
        return static_cast<int64_t>(as_expr.array()->actual_size());
      }

      case ExprKind::ArrayElem: {
        auto& ae = static_cast<ArrayElemExpr&>(*e);
        auto idx = static_cast<std::size_t>(eval(ae.index(), bindings));
        auto* arr = ae.array();
        if (idx < arr->num_elem_vars()) {
          return arr->elem_var(idx).read_as_int64();
        }
        return 0;  // out-of-bounds → 0 (constraint will fail elsewhere)
      }

      case ExprKind::Call: {
        auto& call = static_cast<CallExpr&>(*e);
        std::vector<int64_t> args;
        args.reserve(call.args().size());
        for (auto* arg : call.args()) {
          args.push_back(eval(arg, bindings));
        }
        return call.invoke(args);
      }

      default:
        // ForEachI and Unique should not be evaluated as int — they go through eval_bool
        return 0;
    }
  }

  /// Evaluate an expression as a boolean constraint.
  /// Handles ForEachI and Unique specially; for everything else, eval() != 0.
  bool eval_bool(Expr* e, const Bindings& bindings) {
    switch (e->kind()) {
      case ExprKind::ForEachI:
        return eval_for_each_i(static_cast<ForEachIExpr&>(*e), bindings);

      case ExprKind::Unique:
        return eval_unique(static_cast<UniqueExpr&>(*e), bindings);

      default:
        return eval(e, bindings) != 0;
    }
  }

  bool eval_for_each_i(ForEachIExpr& fe, const Bindings& parent_bindings) {
    auto* arr = fe.array();
    std::size_t n = arr->num_elem_vars();

    for (std::size_t i = 0; i < n; i++) {
      Bindings bindings = parent_bindings;
      bindings[fe.sym_idx()] = static_cast<int64_t>(i);
      bindings[fe.sym_elem()] = arr->elem_var(i).read_as_int64();

      for (auto* body_expr : fe.body()) {
        if (!eval_bool(body_expr, bindings))
          return false;
      }
    }
    return true;
  }

  bool eval_unique(UniqueExpr& uniq, const Bindings& /*bindings*/) {
    auto* arr = uniq.array();
    std::size_t n = arr->num_elem_vars();
    for (std::size_t i = 0; i < n; i++) {
      for (std::size_t j = i + 1; j < n; j++) {
        if (arr->elem_var(i).read_as_int64() == arr->elem_var(j).read_as_int64())
          return false;
      }
    }
    return true;
  }

  // --- Helpers ---

  template <typename Cmp>
  bool eval_cmp(Expr* e, const Bindings& bindings) {
    auto& b = as<BinaryExpr>(e);
    return Cmp{}(eval(b.lhs(), bindings), eval(b.rhs(), bindings));
  }

  template <typename T>
  static T& as(Expr* e) {
    return static_cast<T&>(*e);
  }

  static int64_t bool_to_int(bool v) {
    return v ? 1 : 0;
  }
};

}  // namespace fuzzy

#endif  // FUZZY_EVALUATOR_H
