#ifndef FUZZY_SOLVE_STRATEGY_H
#define FUZZY_SOLVE_STRATEGY_H

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "fuzzy/ConstraintCollector.h"
#include "fuzzy/schedule/ScheduleTypes.h"
#include "fuzzy/ast/Expr.h"
#include "fuzzy/ast/RandArray.h"
#include "fuzzy/ast/RandVar.h"
#include "fuzzy/solver/Z3Solver.h"

namespace fuzzy::schedule {
class ScheduleEngine;
class ScheduleGraphBuilder;
}  // namespace fuzzy::schedule

namespace fuzzy {

namespace solve_randc_detail {

inline void add_randc_disequalities(Z3Solver& solver, const ConstraintCollector& cc) {
  for (VarExprBase* vb : cc.all_vars()) {
    if (vb == nullptr) {
      continue;
    }
    if (vb->compile_kind() != CompileRandKind::Randc) {
      continue;
    }
    if (!cc.is_call_decision_var(*vb)) {
      continue;
    }
    RandcCycle* rc = vb->randc_cycle();
    if (rc == nullptr) {
      continue;
    }
    for (int64_t u : rc->used_values()) {
      auto& ne = detail::make_binary(ExprKind::NotEqual, *vb, detail::make_const(u));
      solver.add_constraint(ne);
    }
  }
}

inline void clear_all_randc_cycles(ConstraintCollector& cc) {
  for (VarExprBase* vb : cc.all_vars()) {
    if (RandcCycle* rc = vb->randc_cycle()) {
      rc->clear_permutation();
    }
  }
}

inline void commit_randc_draws(ConstraintCollector& cc) {
  for (VarExprBase* vb : cc.all_vars()) {
    if (vb->is_symbolic()) {
      continue;
    }
    if (!cc.is_call_decision_var(*vb)) {
      continue;
    }
    if (vb->compile_kind() != CompileRandKind::Randc) {
      continue;
    }
    RandcCycle* rc = vb->randc_cycle();
    if (rc == nullptr) {
      continue;
    }
    rc->record_draw(vb->read_as_int64());
  }
}

}  // namespace solve_randc_detail

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
  bool solve(ConstraintCollector& cc, unsigned seed = 0);

 private:
  friend class schedule::ScheduleEngine;
  friend class schedule::ScheduleGraphBuilder;

  bool schedule_apply_pins(Z3Solver& solver, ConstraintCollector& cc);
  void register_scalar_vars(Z3Solver& solver, ConstraintCollector& cc);
  bool schedule_scalar_no_call(ConstraintCollector& cc, unsigned seed,
                               const schedule::SchedulePayload& payload);
  bool schedule_scalar_with_call(ConstraintCollector& cc, unsigned seed,
                                 const schedule::SchedulePayload& payload);
  void schedule_commit_randc(ConstraintCollector& cc);
  void schedule_materialize(ConstraintCollector& cc);
  bool schedule_elements(ConstraintCollector& cc, unsigned seed,
                         const schedule::SchedulePayload& payload);
  bool schedule_finalize_maps(ConstraintCollector& cc);

  bool block_is_overridden(const std::vector<ConstraintCollector::Block>& blocks,
                           std::size_t idx) const {
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

  bool soft_is_disabled(Expr* soft) const {
    for (auto* disabled : disabled_soft_constraints_) {
      if (expr_structurally_equal(soft, disabled)) {
        return true;
      }
    }
    return false;
  }


  bool expr_structurally_equal(Expr* lhs, Expr* rhs) const {
    if (lhs == rhs) {
      return true;
    }
    if (!lhs || !rhs || lhs->kind() != rhs->kind()) {
      return false;
    }
    switch (lhs->kind()) {
      case ExprKind::Var: {
        auto* lv = dynamic_cast<VarExprBase*>(lhs);
        auto* rv = dynamic_cast<VarExprBase*>(rhs);
        return lv && rv && lv->name() == rv->name();
      }
      case ExprKind::Const:
        return static_cast<ConstExpr*>(lhs)->as_int() == static_cast<ConstExpr*>(rhs)->as_int();
      case ExprKind::Add:
      case ExprKind::Sub:
      case ExprKind::Mul:
      case ExprKind::Div:
      case ExprKind::Mod:
      case ExprKind::Less:
      case ExprKind::LessEqual:
      case ExprKind::Greater:
      case ExprKind::GreaterEqual:
      case ExprKind::Equal:
      case ExprKind::NotEqual:
      case ExprKind::LogicalAnd:
      case ExprKind::LogicalOr: {
        auto& lb = static_cast<BinaryExpr&>(*lhs);
        auto& rb = static_cast<BinaryExpr&>(*rhs);
        return expr_structurally_equal(lb.lhs(), rb.lhs()) &&
            expr_structurally_equal(lb.rhs(), rb.rhs());
      }
      case ExprKind::Abs:
      case ExprKind::LogicalNot: {
        auto& lu = static_cast<UnaryExpr&>(*lhs);
        auto& ru = static_cast<UnaryExpr&>(*rhs);
        return expr_structurally_equal(lu.operand(), ru.operand());
      }
      case ExprKind::Implies: {
        auto& li = static_cast<ImpliesExpr&>(*lhs);
        auto& ri = static_cast<ImpliesExpr&>(*rhs);
        return expr_structurally_equal(li.cond(), ri.cond()) &&
            expr_structurally_equal(li.body(), ri.body());
      }
      case ExprKind::Ite: {
        auto& li = static_cast<IteExpr&>(*lhs);
        auto& ri = static_cast<IteExpr&>(*rhs);
        return expr_structurally_equal(li.cond(), ri.cond()) &&
            expr_structurally_equal(li.then_expr(), ri.then_expr()) &&
            expr_structurally_equal(li.else_expr(), ri.else_expr());
      }
      case ExprKind::ArraySize: {
        auto& la = static_cast<ArraySizeExpr&>(*lhs);
        auto& ra = static_cast<ArraySizeExpr&>(*rhs);
        return la.array() == ra.array();
      }
      case ExprKind::ArrayElem: {
        auto& la = static_cast<ArrayElemExpr&>(*lhs);
        auto& ra = static_cast<ArrayElemExpr&>(*rhs);
        return la.array() == ra.array() && expr_structurally_equal(la.index(), ra.index());
      }
      case ExprKind::ArrayAgg: {
        auto& la = static_cast<ArrayAggExpr&>(*lhs);
        auto& ra = static_cast<ArrayAggExpr&>(*rhs);
        if (la.agg_kind() != ra.agg_kind() || la.array() != ra.array() ||
            la.has_with() != ra.has_with()) {
          return false;
        }
        if (!la.has_with()) {
          return true;
        }
        return expr_structurally_equal(la.value_expr(), ra.value_expr());
      }
      default:
        return lhs == rhs;
    }
  }

  bool apply_solve_before_hint(Z3Solver& solver, const ConstraintCollector& cc) {
    auto ordered = collect_solve_before_vars(cc);
    for (auto* expr : ordered) {
      auto* var = dynamic_cast<VarExprBase*>(expr);
      if (!var || var->is_symbolic()) {
        continue;
      }
      if (var->compile_kind() == CompileRandKind::Randc) {
        return false;
      }
      if (!solver.solve()) {
        return false;
      }
      auto z3var = solver.get_or_create_var(*var);
      auto model = solver.solver().get_model();
      auto val = model.eval(z3var, true);
      solver.solver().add(z3var == val);
    }
    return true;
  }

  std::vector<Expr*> collect_solve_before_vars(const ConstraintCollector& cc) {
    std::vector<Expr*> ordered;
    std::unordered_set<Expr*> seen;
    for (const auto& [lhs, rhs] : cc.solve_before()) {
      if (lhs && seen.insert(lhs).second) {
        ordered.push_back(lhs);
      }
      if (rhs && seen.insert(rhs).second) {
        ordered.push_back(rhs);
      }
    }
    return ordered;
  }

  void shuffle_soft_constraints(std::vector<Expr*>& soft_constraints, unsigned seed) {
    (void)soft_constraints;
    (void)seed;
    // Keep source declaration order for greedy soft-constraint insertion.
    // This makes unsat soft constraints drop in reverse text order.
  }

  void add_soft_constraints_greedily(Z3Solver& solver, const std::vector<Expr*>& soft_constraints) {
    for (auto* c : soft_constraints) {
      solver.solver().push();
      solver.add_constraint(*c);
      const bool sat = solver.solve();
      solver.solver().pop();
      if (sat) {
        solver.add_constraint(*c);
      }
    }
  }

  void apply_dist_constraints(Z3Solver& solver,
                              const std::vector<ConstraintCollector::DistConstraint>& constraints,
                              unsigned seed) {
    for (std::size_t i = 0; i < constraints.size(); ++i) {
      if (auto pinned = choose_dist_value(solver, constraints[i], seed + static_cast<unsigned>(i))) {
        pin_dist_value(solver, *constraints[i].expr, *pinned);
      }
    }
  }

  std::optional<int64_t> choose_dist_value(Z3Solver& solver,
                                           const ConstraintCollector::DistConstraint& dist,
                                           unsigned seed) {
    if (!dist.expr) {
      return std::nullopt;
    }
    std::vector<std::pair<int64_t, int>> feasible;
    feasible.reserve(dist.weighted_values.size());
    for (const auto& [value, weight] : dist.weighted_values) {
      if (weight <= 0) {
        continue;
      }
      solver.solver().push();
      solver.solver().add(solver.translate(*dist.expr) == solver.context().int_val(value));
      const bool sat = solver.solve();
      solver.solver().pop();
      if (sat) {
        feasible.emplace_back(value, weight);
      }
    }
    if (feasible.empty()) {
      return std::nullopt;
    }
    int total = 0;
    for (const auto& [_, weight] : feasible) {
      total += weight;
    }
    if (total <= 0) {
      return std::nullopt;
    }
    unsigned pick = seed_for_dist_choice(seed);
    int slot = static_cast<int>(pick % total);
    for (const auto& [value, weight] : feasible) {
      if (slot < weight) {
        return value;
      }
      slot -= weight;
    }
    return feasible.back().first;
  }

  void pin_dist_value(Z3Solver& solver, Expr& expr, int64_t value) {
    solver.solver().add(solver.translate(expr) == solver.context().int_val(value));
  }

  unsigned seed_for_dist_choice(unsigned seed) const {
    seed ^= seed >> 16;
    seed *= 0x7feb352du;
    seed ^= seed >> 15;
    seed *= 0x846ca68bu;
    seed ^= seed >> 16;
    return seed;
  }

  // Check if an expression involves array elements
  bool involves_array_elements(Expr* e) {
    if (!e)
      return false;
    switch (e->kind()) {
      case ExprKind::ForEachI:
      case ExprKind::ForEachKV:
      case ExprKind::Unique:
      case ExprKind::ArrayElem:
      case ExprKind::ArrayAgg:
        return true;
      case ExprKind::Add:
      case ExprKind::Sub:
      case ExprKind::Mul:
      case ExprKind::Div:
      case ExprKind::Mod:
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
      case ExprKind::Ite: {
        auto& it = static_cast<IteExpr&>(*e);
        return involves_array_elements(it.cond()) || involves_array_elements(it.then_expr()) ||
            involves_array_elements(it.else_expr());
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
    } else if (e->kind() == ExprKind::ForEachKV) {
      auto& fe = static_cast<ForEachKVExpr&>(*e);
      auto* map = fe.map();
      std::size_t n = map->num_entries();

      for (std::size_t i = 0; i < n; i++) {
        auto& key_i = map->key_var(i);
        auto& val_i = map->value_var(i);
        for (auto* body_expr : fe.body()) {
          auto* expanded = substitute_kv(body_expr, fe.sym_idx(), fe.sym_key(), fe.sym_value(), i,
                                         &key_i, &val_i, cc);
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
      case ExprKind::Mul:
      case ExprKind::Div:
      case ExprKind::Mod:
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

      case ExprKind::Ite: {
        auto& it = static_cast<IteExpr&>(*e);
        auto* nc = substitute(it.cond(), sym_idx, sym_elem, concrete_idx, concrete_elem, cc);
        auto* nt = substitute(it.then_expr(), sym_idx, sym_elem, concrete_idx, concrete_elem, cc);
        auto* ne = substitute(it.else_expr(), sym_idx, sym_elem, concrete_idx, concrete_elem, cc);
        if (nc == it.cond() && nt == it.then_expr() && ne == it.else_expr())
          return e;
        return &detail::make_ite(*nc, *nt, *ne);
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

      case ExprKind::ForEachKV: {
        auto& inner = static_cast<ForEachKVExpr&>(*e);
        std::vector<Expr*> new_body;
        for (auto* body_expr : inner.body()) {
          new_body.push_back(
              substitute(body_expr, sym_idx, sym_elem, concrete_idx, concrete_elem, cc));
        }
        auto ptr = std::make_unique<ForEachKVExpr>(inner.map(), inner.sym_idx(), inner.sym_key(),
                                                   inner.sym_value(), std::move(new_body));
        auto* result = ptr.get();
        cc.arena().nodes.push_back(std::move(ptr));
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

  Expr* substitute_kv(Expr* e, Expr* sym_idx, Expr* sym_key, Expr* sym_val, std::size_t concrete_idx,
                      VarExprBase* concrete_key, VarExprBase* concrete_val, ConstraintCollector& cc) {
    if (!e)
      return nullptr;

    if (e == sym_idx) {
      return &detail::make_const(static_cast<int64_t>(concrete_idx));
    }
    if (e == sym_key) {
      return concrete_key;
    }
    if (e == sym_val) {
      return concrete_val;
    }

    switch (e->kind()) {
      case ExprKind::Var:
      case ExprKind::Const:
        return e;

      case ExprKind::Add:
      case ExprKind::Sub:
      case ExprKind::Mul:
      case ExprKind::Div:
      case ExprKind::Mod:
      case ExprKind::Less:
      case ExprKind::LessEqual:
      case ExprKind::Greater:
      case ExprKind::GreaterEqual:
      case ExprKind::Equal:
      case ExprKind::NotEqual:
      case ExprKind::LogicalAnd:
      case ExprKind::LogicalOr: {
        auto& b = static_cast<BinaryExpr&>(*e);
        auto* new_lhs =
            substitute_kv(b.lhs(), sym_idx, sym_key, sym_val, concrete_idx, concrete_key, concrete_val, cc);
        auto* new_rhs =
            substitute_kv(b.rhs(), sym_idx, sym_key, sym_val, concrete_idx, concrete_key, concrete_val, cc);
        if (new_lhs == b.lhs() && new_rhs == b.rhs())
          return e;
        return &detail::make_binary(e->kind(), *new_lhs, *new_rhs);
      }

      case ExprKind::Abs:
      case ExprKind::LogicalNot: {
        auto& u = static_cast<UnaryExpr&>(*e);
        auto* new_op =
            substitute_kv(u.operand(), sym_idx, sym_key, sym_val, concrete_idx, concrete_key, concrete_val, cc);
        if (new_op == u.operand())
          return e;
        return &detail::make_unary(e->kind(), *new_op);
      }

      case ExprKind::Implies: {
        auto& imp = static_cast<ImpliesExpr&>(*e);
        auto* new_cond =
            substitute_kv(imp.cond(), sym_idx, sym_key, sym_val, concrete_idx, concrete_key, concrete_val, cc);
        auto* new_body =
            substitute_kv(imp.body(), sym_idx, sym_key, sym_val, concrete_idx, concrete_key, concrete_val, cc);
        if (new_cond == imp.cond() && new_body == imp.body())
          return e;
        return &cc.make_implies(*new_cond, *new_body);
      }

      case ExprKind::Ite: {
        auto& it = static_cast<IteExpr&>(*e);
        auto* nc =
            substitute_kv(it.cond(), sym_idx, sym_key, sym_val, concrete_idx, concrete_key, concrete_val, cc);
        auto* nt = substitute_kv(it.then_expr(), sym_idx, sym_key, sym_val, concrete_idx, concrete_key,
                                  concrete_val, cc);
        auto* ne = substitute_kv(it.else_expr(), sym_idx, sym_key, sym_val, concrete_idx, concrete_key,
                                 concrete_val, cc);
        if (nc == it.cond() && nt == it.then_expr() && ne == it.else_expr())
          return e;
        return &detail::make_ite(*nc, *nt, *ne);
      }

      case ExprKind::ForEachI: {
        auto& inner = static_cast<ForEachIExpr&>(*e);
        std::vector<Expr*> new_body;
        for (auto* body_expr : inner.body()) {
          new_body.push_back(substitute_kv(body_expr, sym_idx, sym_key, sym_val, concrete_idx,
                                          concrete_key, concrete_val, cc));
        }
        auto ptr = std::make_unique<ForEachIExpr>(inner.array(), inner.sym_idx(), inner.sym_elem(),
                                                  std::move(new_body));
        auto* result = ptr.get();
        cc.arena().nodes.push_back(std::move(ptr));
        return result;
      }

      case ExprKind::ForEachKV: {
        auto& inner = static_cast<ForEachKVExpr&>(*e);
        std::vector<Expr*> new_body;
        for (auto* body_expr : inner.body()) {
          new_body.push_back(substitute_kv(body_expr, sym_idx, sym_key, sym_val, concrete_idx,
                                          concrete_key, concrete_val, cc));
        }
        auto ptr = std::make_unique<ForEachKVExpr>(inner.map(), inner.sym_idx(), inner.sym_key(),
                                                   inner.sym_value(), std::move(new_body));
        auto* result = ptr.get();
        cc.arena().nodes.push_back(std::move(ptr));
        return result;
      }

      case ExprKind::ArrayElem: {
        auto& ae = static_cast<ArrayElemExpr&>(*e);
        auto* new_idx =
            substitute_kv(ae.index(), sym_idx, sym_key, sym_val, concrete_idx, concrete_key, concrete_val, cc);
        if (new_idx == ae.index())
          return e;

        if (new_idx->kind() == ExprKind::Const) {
          auto& c = static_cast<ConstExpr&>(*new_idx);
          std::size_t idx = static_cast<std::size_t>(c.as_int());
          if (idx < ae.array()->num_elem_vars()) {
            return &ae.array()->elem_var(idx);
          }
        }
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
          auto* new_arg =
              substitute_kv(arg, sym_idx, sym_key, sym_val, concrete_idx, concrete_key, concrete_val, cc);
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
      case ExprKind::Mul:
      case ExprKind::Div:
      case ExprKind::Mod:
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
      case ExprKind::Ite: {
        auto& it = static_cast<IteExpr&>(*e);
        return contains_call(it.cond()) || contains_call(it.then_expr()) || contains_call(it.else_expr());
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
      case ExprKind::ForEachKV: {
        auto& fe = static_cast<ForEachKVExpr&>(*e);
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
      case ExprKind::Mul:
      case ExprKind::Div:
      case ExprKind::Mod:
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
      case ExprKind::Ite: {
        auto& it = static_cast<IteExpr&>(*e);
        collect_vars_for_pin(it.cond(), out);
        collect_vars_for_pin(it.then_expr(), out);
        collect_vars_for_pin(it.else_expr(), out);
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
      case ExprKind::ForEachKV: {
        auto& fe = static_cast<ForEachKVExpr&>(*e);
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
      case ExprKind::Mul:
      case ExprKind::Div:
      case ExprKind::Mod:
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
      case ExprKind::Ite: {
        auto& it = static_cast<IteExpr&>(*e);
        collect_call_argument_vars(it.cond(), out);
        collect_call_argument_vars(it.then_expr(), out);
        collect_call_argument_vars(it.else_expr(), out);
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
      case ExprKind::ForEachKV: {
        auto& fe = static_cast<ForEachKVExpr&>(*e);
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
      case ExprKind::Mul:
      case ExprKind::Div:
      case ExprKind::Mod:
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
          case ExprKind::Mul:
            out = lhs * rhs;
            return true;
          case ExprKind::Div:
            if (rhs == 0) {
              return false;
            }
            out = lhs / rhs;
            return true;
          case ExprKind::Mod:
            if (rhs == 0) {
              return false;
            }
            out = lhs % rhs;
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
      case ExprKind::Ite: {
        auto& it = static_cast<IteExpr&>(*e);
        int64_t c = 0;
        int64_t t = 0;
        int64_t el = 0;
        if (!try_eval_scalar_expr(it.cond(), c) || !try_eval_scalar_expr(it.then_expr(), t) ||
            !try_eval_scalar_expr(it.else_expr(), el)) {
          return false;
        }
        out = (c != 0) ? t : el;
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
      case ExprKind::Mul:
      case ExprKind::Div:
      case ExprKind::Mod:
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
      case ExprKind::Ite: {
        auto& it = static_cast<IteExpr&>(*e);
        auto* c = reduce_calls(it.cond(), cc);
        auto* t = reduce_calls(it.then_expr(), cc);
        auto* el = reduce_calls(it.else_expr(), cc);
        if (c == it.cond() && t == it.then_expr() && el == it.else_expr()) {
          return e;
        }
        return &detail::make_ite(*c, *t, *el);
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
      case ExprKind::ForEachKV: {
        auto& fe = static_cast<ForEachKVExpr&>(*e);
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
        auto ptr = std::make_unique<ForEachKVExpr>(fe.map(), fe.sym_idx(), fe.sym_key(), fe.sym_value(),
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

  void apply_non_decision_var_pins(Z3Solver& solver, const ConstraintCollector& cc) {
    for (auto* var : cc.all_vars()) {
      auto* vb = dynamic_cast<VarExprBase*>(var);
      if (!vb || vb->is_symbolic()) {
        continue;
      }
      if (cc.is_call_decision_var(*vb)) {
        continue;
      }
      pin_solved_expr(solver, *vb);
    }
  }

  /// Replace `ArrayAgg` with `Add` / `ite`-folded min/max once element vars exist.
  static Expr* expand_array_agg_node(ArrayAggKind k, ArrayExprBase* arr,
                                     const std::vector<Expr*>* values = nullptr) {
    const std::size_t n = arr->num_elem_vars();
    auto value_at = [&](std::size_t i) -> Expr& {
      return values ? *(*values)[i] : static_cast<Expr&>(arr->elem_var(i));
    };
    if (k == ArrayAggKind::Sum) {
      if (n == 0) {
        return &detail::make_const(0);
      }
      Expr* acc = &value_at(0);
      for (std::size_t i = 1; i < n; ++i) {
        acc = &detail::make_binary(ExprKind::Add, *acc, value_at(i));
      }
      return acc;
    }
    if (k == ArrayAggKind::Product) {
      if (n == 0) {
        return &detail::make_const(1);
      }
      Expr* acc = &value_at(0);
      for (std::size_t i = 1; i < n; ++i) {
        acc = &detail::make_binary(ExprKind::Mul, *acc, value_at(i));
      }
      return acc;
    }
    if (k == ArrayAggKind::And) {
      if (n == 0) {
        return &detail::make_const(1);
      }
      Expr* acc = &detail::make_binary(ExprKind::NotEqual, value_at(0), detail::make_const(0));
      for (std::size_t i = 1; i < n; ++i) {
        auto& elem_true = detail::make_binary(ExprKind::NotEqual, value_at(i), detail::make_const(0));
        acc = &detail::make_binary(ExprKind::LogicalAnd, *acc, elem_true);
      }
      return &detail::make_ite(*acc, detail::make_const(1), detail::make_const(0));
    }
    if (k == ArrayAggKind::Or) {
      if (n == 0) {
        return &detail::make_const(0);
      }
      Expr* acc = &detail::make_binary(ExprKind::NotEqual, value_at(0), detail::make_const(0));
      for (std::size_t i = 1; i < n; ++i) {
        auto& elem_true = detail::make_binary(ExprKind::NotEqual, value_at(i), detail::make_const(0));
        acc = &detail::make_binary(ExprKind::LogicalOr, *acc, elem_true);
      }
      return &detail::make_ite(*acc, detail::make_const(1), detail::make_const(0));
    }
    if (k == ArrayAggKind::Xor) {
      if (n == 0) {
        return &detail::make_const(0);
      }
      Expr* acc = &detail::make_binary(ExprKind::NotEqual, value_at(0), detail::make_const(0));
      for (std::size_t i = 1; i < n; ++i) {
        auto& rhs_true = detail::make_binary(ExprKind::NotEqual, value_at(i), detail::make_const(0));
        acc = &detail::make_binary(ExprKind::NotEqual, *acc, rhs_true);
      }
      return &detail::make_ite(*acc, detail::make_const(1), detail::make_const(0));
    }
    if (n == 0) {
      return &detail::make_binary(ExprKind::Equal, detail::make_const(0), detail::make_const(1));
    }
    if (k == ArrayAggKind::Min) {
      Expr* acc = &value_at(0);
      for (std::size_t i = 1; i < n; ++i) {
        auto& value = value_at(i);
        auto& le = detail::make_binary(ExprKind::LessEqual, *acc, value);
        acc = &detail::make_ite(le, *acc, value);
      }
      return acc;
    }
    Expr* acc = &value_at(0);
    for (std::size_t i = 1; i < n; ++i) {
      auto& value = value_at(i);
      auto& ge = detail::make_binary(ExprKind::GreaterEqual, *acc, value);
      acc = &detail::make_ite(ge, *acc, value);
    }
    return acc;
  }

  Expr* rewrite_array_aggs(Expr* e, ConstraintCollector& cc) {
    if (!e) {
      return nullptr;
    }
    switch (e->kind()) {
      case ExprKind::ArrayAgg: {
        auto& ag = static_cast<ArrayAggExpr&>(*e);
        if (!ag.has_with()) {
          return expand_array_agg_node(ag.agg_kind(), ag.array());
        }
        std::vector<Expr*> values;
        values.reserve(ag.array()->num_elem_vars());
        for (std::size_t i = 0; i < ag.array()->num_elem_vars(); ++i) {
          auto* expanded = substitute(ag.value_expr(), ag.sym_idx(), ag.sym_elem(), i,
                                      &ag.array()->elem_var(i), cc);
          values.push_back(rewrite_array_aggs(expanded, cc));
        }
        return expand_array_agg_node(ag.agg_kind(), ag.array(), &values);
      }
      case ExprKind::Add:
      case ExprKind::Sub:
      case ExprKind::Mul:
      case ExprKind::Div:
      case ExprKind::Mod:
      case ExprKind::Less:
      case ExprKind::LessEqual:
      case ExprKind::Greater:
      case ExprKind::GreaterEqual:
      case ExprKind::Equal:
      case ExprKind::NotEqual:
      case ExprKind::LogicalAnd:
      case ExprKind::LogicalOr: {
        auto& b = static_cast<BinaryExpr&>(*e);
        Expr* nl = rewrite_array_aggs(b.lhs(), cc);
        Expr* nr = rewrite_array_aggs(b.rhs(), cc);
        if (nl == b.lhs() && nr == b.rhs()) {
          return e;
        }
        return &detail::make_binary(e->kind(), *nl, *nr);
      }
      case ExprKind::Abs:
      case ExprKind::LogicalNot: {
        auto& u = static_cast<UnaryExpr&>(*e);
        Expr* no = rewrite_array_aggs(u.operand(), cc);
        if (no == u.operand()) {
          return e;
        }
        return &detail::make_unary(e->kind(), *no);
      }
      case ExprKind::Implies: {
        auto& imp = static_cast<ImpliesExpr&>(*e);
        Expr* nc = rewrite_array_aggs(imp.cond(), cc);
        Expr* nb = rewrite_array_aggs(imp.body(), cc);
        if (nc == imp.cond() && nb == imp.body()) {
          return e;
        }
        return &cc.make_implies(*nc, *nb);
      }
      case ExprKind::Ite: {
        auto& it = static_cast<IteExpr&>(*e);
        Expr* c = rewrite_array_aggs(it.cond(), cc);
        Expr* t = rewrite_array_aggs(it.then_expr(), cc);
        Expr* el = rewrite_array_aggs(it.else_expr(), cc);
        if (c == it.cond() && t == it.then_expr() && el == it.else_expr()) {
          return e;
        }
        return &detail::make_ite(*c, *t, *el);
      }
      case ExprKind::Call: {
        auto& call = static_cast<CallExpr&>(*e);
        bool changed = false;
        std::vector<Expr*> nargs;
        nargs.reserve(call.args().size());
        for (auto* arg : call.args()) {
          Expr* ra = rewrite_array_aggs(arg, cc);
          changed = changed || (ra != arg);
          nargs.push_back(ra);
        }
        if (!changed) {
          return e;
        }
        auto eval_fn = [&call](const std::vector<int64_t>& concrete_args) -> int64_t {
          return call.invoke(concrete_args);
        };
        return &detail::make_call(std::move(nargs), std::move(eval_fn));
      }
      case ExprKind::ArrayElem: {
        auto& ae = static_cast<ArrayElemExpr&>(*e);
        Expr* ni = rewrite_array_aggs(ae.index(), cc);
        if (ni == ae.index()) {
          return e;
        }
        auto ptr = std::make_unique<ArrayElemExpr>(ae.array(), ni);
        auto* out = ptr.get();
        cc.arena().nodes.push_back(std::move(ptr));
        return out;
      }
      case ExprKind::ForEachI: {
        auto& fe = static_cast<ForEachIExpr&>(*e);
        bool changed = false;
        std::vector<Expr*> nb;
        nb.reserve(fe.body().size());
        for (auto* be : fe.body()) {
          Expr* r = rewrite_array_aggs(be, cc);
          changed = changed || (r != be);
          nb.push_back(r);
        }
        if (!changed) {
          return e;
        }
        auto ptr = std::make_unique<ForEachIExpr>(fe.array(), fe.sym_idx(), fe.sym_elem(), std::move(nb));
        auto* out = ptr.get();
        cc.arena().nodes.push_back(std::move(ptr));
        return out;
      }
      case ExprKind::ForEachKV: {
        auto& fe = static_cast<ForEachKVExpr&>(*e);
        bool changed = false;
        std::vector<Expr*> nb;
        nb.reserve(fe.body().size());
        for (auto* be : fe.body()) {
          Expr* r = rewrite_array_aggs(be, cc);
          changed = changed || (r != be);
          nb.push_back(r);
        }
        if (!changed) {
          return e;
        }
        auto ptr = std::make_unique<ForEachKVExpr>(fe.map(), fe.sym_idx(), fe.sym_key(), fe.sym_value(),
                                                   std::move(nb));
        auto* out = ptr.get();
        cc.arena().nodes.push_back(std::move(ptr));
        return out;
      }
      default:
        return e;
    }
  }

  std::vector<Expr*> disabled_soft_constraints_;
};

}  // namespace fuzzy

#endif  // FUZZY_SOLVE_STRATEGY_H
