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
    for (auto* map_expr : cc.all_maps()) {
      std::size_t sz = map_expr->actual_size();
      map_expr->materialize_entries(sz);
      if (!map_expr->check_key_uniqueness()) {
        return false;
      }
    }

    // Evaluate every constraint in every block
    Bindings empty;
    const auto& blocks = cc.blocks();
    for (std::size_t block_idx = 0; block_idx < blocks.size(); ++block_idx) {
      const auto& block = blocks[block_idx];
      if (block_is_overridden(blocks, block_idx)) {
        continue;
      }
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
      case ExprKind::Mul: {
        auto& b = as<BinaryExpr>(e);
        return eval(b.lhs(), bindings) * eval(b.rhs(), bindings);
      }
      case ExprKind::Div: {
        auto& b = as<BinaryExpr>(e);
        return eval(b.lhs(), bindings) / eval(b.rhs(), bindings);
      }
      case ExprKind::Mod: {
        auto& b = as<BinaryExpr>(e);
        return eval(b.lhs(), bindings) % eval(b.rhs(), bindings);
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

      case ExprKind::Ite: {
        auto& it = as<IteExpr>(e);
        return eval(it.cond(), bindings) != 0 ? eval(it.then_expr(), bindings) : eval(it.else_expr(), bindings);
      }

      case ExprKind::ArrayAgg: {
        auto& ag = as<ArrayAggExpr>(e);
        auto* arr = ag.array();
        const std::size_t n = arr->num_elem_vars();
        switch (ag.agg_kind()) {
          case ArrayAggKind::Sum: {
            int64_t s = 0;
            for (std::size_t i = 0; i < n; ++i) {
              s += eval_array_agg_value(ag, i, bindings);
            }
            return s;
          }
          case ArrayAggKind::Product: {
            int64_t p = 1;
            for (std::size_t i = 0; i < n; ++i) {
              p *= eval_array_agg_value(ag, i, bindings);
            }
            return p;
          }
          case ArrayAggKind::And: {
            int64_t v = 1;
            for (std::size_t i = 0; i < n; ++i) {
              v = (v != 0) && (eval_array_agg_value(ag, i, bindings) != 0);
            }
            return v;
          }
          case ArrayAggKind::Or: {
            int64_t v = 0;
            for (std::size_t i = 0; i < n; ++i) {
              v = (v != 0) || (eval_array_agg_value(ag, i, bindings) != 0);
            }
            return v;
          }
          case ArrayAggKind::Xor: {
            int64_t v = 0;
            for (std::size_t i = 0; i < n; ++i) {
              v = (v != 0) != (eval_array_agg_value(ag, i, bindings) != 0);
            }
            return v;
          }
          case ArrayAggKind::Min: {
            if (n == 0) {
              return 0;
            }
            int64_t m = eval_array_agg_value(ag, 0, bindings);
            for (std::size_t i = 1; i < n; ++i) {
              int64_t v = eval_array_agg_value(ag, i, bindings);
              if (v < m) {
                m = v;
              }
            }
            return m;
          }
          case ArrayAggKind::Max: {
            if (n == 0) {
              return 0;
            }
            int64_t m = eval_array_agg_value(ag, 0, bindings);
            for (std::size_t i = 1; i < n; ++i) {
              int64_t v = eval_array_agg_value(ag, i, bindings);
              if (v > m) {
                m = v;
              }
            }
            return m;
          }
        }
        return 0;
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
        // ForEachI / ForEachKV / Unique should not be evaluated as int — they go through eval_bool
        return 0;
    }
  }

  /// Evaluate an expression as a boolean constraint.
  /// Handles ForEachI and Unique specially; for everything else, eval() != 0.
  bool eval_bool(Expr* e, const Bindings& bindings) {
    switch (e->kind()) {
      case ExprKind::ForEachI:
        return eval_for_each_i(static_cast<ForEachIExpr&>(*e), bindings);

      case ExprKind::ForEachKV:
        return eval_for_each_kv(static_cast<ForEachKVExpr&>(*e), bindings);

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

  bool eval_for_each_kv(ForEachKVExpr& fe, const Bindings& parent_bindings) {
    auto* map = fe.map();
    std::size_t n = map->num_entries();

    for (std::size_t i = 0; i < n; i++) {
      Bindings bindings = parent_bindings;
      bindings[fe.sym_idx()] = static_cast<int64_t>(i);
      bindings[fe.sym_key()] = map->key_var(i).read_as_int64();
      bindings[fe.sym_value()] = map->value_var(i).read_as_int64();

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

  static bool block_is_overridden(const std::vector<ConstraintCollector::Block>& blocks,
                                  std::size_t idx) {
    const auto& name = blocks[idx].name;
    if (name == "default") {
      return false;
    }
    for (std::size_t i = idx + 1; i < blocks.size(); ++i) {
      if (blocks[i].name == name) {
        return true;
      }
    }
    return false;
  }

  int64_t eval_array_agg_value(ArrayAggExpr& ag, std::size_t i, const Bindings& parent_bindings) {
    auto* arr = ag.array();
    if (!ag.has_with()) {
      return arr->elem_var(i).read_as_int64();
    }
    Bindings bindings = parent_bindings;
    bindings[ag.sym_idx()] = static_cast<int64_t>(i);
    bindings[ag.sym_elem()] = arr->elem_var(i).read_as_int64();
    return eval(ag.value_expr(), bindings);
  }

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
