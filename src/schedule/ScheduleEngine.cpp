#include "fuzzy/schedule/ScheduleEngine.h"

#include "fuzzy/SolveStrategy.h"
#include "fuzzy/ast/RandArray.h"
#include "fuzzy/solver/Z3Solver.h"

namespace fuzzy::schedule {

bool ScheduleEngine::run(ConstraintCollector& cc, unsigned seed) {
  strategy_.disabled_soft_constraints_.clear();

  SchedulePayload payload;
  DependencyGraph graph;
  ScheduleGraphBuilder builder(strategy_, cc, payload, graph);
  builder.build();

  while (!graph.all_done()) {
    const auto ready = graph.ready_nodes();
    if (ready.empty()) {
      return false;
    }
    for (const SchedNodeId id : ready) {
      if (!execute_node(graph.kind(id), cc, seed, payload)) {
        return false;
      }
      graph.mark_done(id);
    }
  }

  return strategy_.schedule_finalize_maps(cc);
}

bool ScheduleEngine::execute_node(SchedNodeKind kind, ConstraintCollector& cc, unsigned seed,
                                  const SchedulePayload& payload) {
  switch (kind) {
    case SchedNodeKind::ApplyPins:
      return true;
    case SchedNodeKind::SolveScalarNoCall:
      return strategy_.schedule_scalar_no_call(cc, seed, payload);
    case SchedNodeKind::SolveScalarWithCall:
      return strategy_.schedule_scalar_with_call(cc, seed, payload);
    case SchedNodeKind::CommitRandc:
      strategy_.schedule_commit_randc(cc);
      return true;
    case SchedNodeKind::MaterializeContainers:
      strategy_.schedule_materialize(cc);
      return true;
    case SchedNodeKind::SolveElements:
      return strategy_.schedule_elements(cc, seed, payload);
  }
  return false;
}

}  // namespace fuzzy::schedule

namespace fuzzy {

bool SolveStrategy::solve(ConstraintCollector& cc, unsigned seed) {
  schedule::ScheduleEngine engine(*this);
  return engine.run(cc, seed);
}

bool SolveStrategy::schedule_apply_pins(Z3Solver& solver, ConstraintCollector& cc) {
  apply_non_decision_var_pins(solver, cc);
  return true;
}

void SolveStrategy::register_scalar_vars(Z3Solver& solver, ConstraintCollector& cc) {
  for (auto* var : cc.all_vars()) {
    solver.get_or_create_var(*var);
  }
  for (auto* arr : cc.all_arrays()) {
    solver.get_or_create_var(arr->size_var_base());
  }
}

bool SolveStrategy::schedule_scalar_no_call(ConstraintCollector& cc, unsigned seed,
                                            const schedule::SchedulePayload& payload) {
  bool phase_ok = false;
  for (int randc_attempt = 0; randc_attempt < 2 && !phase_ok; ++randc_attempt) {
    if (randc_attempt == 1) {
      solve_randc_detail::clear_all_randc_cycles(cc);
    }
    Z3Solver solver(seed);
    register_scalar_vars(solver, cc);
    schedule_apply_pins(solver, cc);

    for (Expr* c : payload.scalar_hard_no_call) {
      solver.add_constraint(*c);
    }
    auto shuffled_soft = payload.scalar_soft_no_call;
    shuffle_soft_constraints(shuffled_soft, seed + 17);
    add_soft_constraints_greedily(solver, shuffled_soft);
    solve_randc_detail::add_randc_disequalities(solver, cc);
    apply_dist_constraints(solver, payload.scalar_dist, seed + 43);
    if (!apply_solve_before_hint(solver, cc)) {
      return false;
    }

    if (solver.solve()) {
      solver.write_back();
      phase_ok = true;
    }
  }
  return phase_ok;
}

bool SolveStrategy::schedule_scalar_with_call(ConstraintCollector& cc, unsigned seed,
                                              const schedule::SchedulePayload& payload) {
  if (!payload.has_scalar_call_phase) {
    return true;
  }

  std::vector<Expr*> reduced_call_constraints;
  reduced_call_constraints.reserve(payload.scalar_hard_with_call.size());
  for (Expr* c : payload.scalar_hard_with_call) {
    Expr* reduced = reduce_calls(c, cc);
    if (!reduced || contains_call(reduced)) {
      return false;
    }
    reduced_call_constraints.push_back(reduced);
  }

  std::vector<Expr*> reduced_soft_call_constraints;
  reduced_soft_call_constraints.reserve(payload.scalar_soft_with_call.size());
  for (Expr* c : payload.scalar_soft_with_call) {
    Expr* reduced = reduce_calls(c, cc);
    if (!reduced || contains_call(reduced)) {
      continue;
    }
    reduced_soft_call_constraints.push_back(reduced);
  }

  bool phase_ok = false;
  for (int randc_attempt = 0; randc_attempt < 2 && !phase_ok; ++randc_attempt) {
    if (randc_attempt == 1) {
      solve_randc_detail::clear_all_randc_cycles(cc);
    }
    Z3Solver solver(seed + 7);
    register_scalar_vars(solver, cc);
    schedule_apply_pins(solver, cc);

    for (Expr* c : payload.scalar_hard_no_call) {
      solver.add_constraint(*c);
    }
    for (Expr* c : reduced_call_constraints) {
      solver.add_constraint(*c);
    }
    auto shuffled_soft = payload.scalar_soft_no_call;
    shuffled_soft.insert(shuffled_soft.end(), reduced_soft_call_constraints.begin(),
                         reduced_soft_call_constraints.end());
    shuffle_soft_constraints(shuffled_soft, seed + 23);
    add_soft_constraints_greedily(solver, shuffled_soft);
    solve_randc_detail::add_randc_disequalities(solver, cc);

    std::unordered_set<Expr*> pinned_vars;
    for (Expr* c : payload.scalar_hard_with_call) {
      collect_call_argument_vars(c, pinned_vars);
    }
    for (Expr* v : pinned_vars) {
      pin_solved_expr(solver, *v);
    }
    apply_dist_constraints(solver, payload.scalar_dist, seed + 43);
    if (!apply_solve_before_hint(solver, cc)) {
      return false;
    }

    if (solver.solve()) {
      solver.write_back();
      phase_ok = true;
    }
  }
  return phase_ok;
}

void SolveStrategy::schedule_commit_randc(ConstraintCollector& cc) {
  solve_randc_detail::commit_randc_draws(cc);
}

void SolveStrategy::schedule_materialize(ConstraintCollector& cc) {
  for (auto* arr : cc.all_arrays()) {
    const std::size_t concrete_size = arr->concrete_size();
    arr->resize(concrete_size);
    arr->materialize_elements(concrete_size);
  }
  for (auto* map_expr : cc.all_maps()) {
    map_expr->materialize_entries(map_expr->concrete_size());
  }
}

bool SolveStrategy::schedule_elements(ConstraintCollector& cc, unsigned seed,
                                      const schedule::SchedulePayload& payload) {
  Z3Solver solver(seed + 1);
  std::unordered_set<Expr*> phase2_scalar_decision_vars;
  for (Expr* c : payload.element_hard) {
    collect_vars_for_pin(c, phase2_scalar_decision_vars);
  }
  for (Expr* c : payload.element_soft) {
    collect_vars_for_pin(c, phase2_scalar_decision_vars);
  }

  for (auto* arr : cc.all_arrays()) {
    for (std::size_t i = 0; i < arr->num_elem_vars(); i++) {
      solver.get_or_create_var(arr->elem_var(i));
      if (cc.decision_vars_are_restricted()) {
        pin_solved_expr(solver, arr->elem_var(i));
      }
    }
  }
  for (auto* map_expr : cc.all_maps()) {
    for (std::size_t i = 0; i < map_expr->num_entries(); i++) {
      solver.get_or_create_var(map_expr->key_var(i));
      solver.get_or_create_var(map_expr->value_var(i));
      if (cc.decision_vars_are_restricted()) {
        pin_solved_expr(solver, map_expr->key_var(i));
        pin_solved_expr(solver, map_expr->value_var(i));
      }
    }
  }

  for (auto* var : cc.all_vars()) {
    solver.get_or_create_var(*var);
  }

  for (Expr* c : payload.scalar_hard_no_call) {
    solver.add_constraint(*c);
  }
  for (auto* var : cc.all_vars()) {
    if (phase2_scalar_decision_vars.contains(var) && cc.is_call_decision_var(*var)) {
      continue;
    }
    pin_solved_expr(solver, *var);
  }

  for (auto* map_expr : cc.all_maps()) {
    const std::size_t n = map_expr->num_entries();
    for (std::size_t i = 0; i < n; i++) {
      for (std::size_t j = i + 1; j < n; j++) {
        auto& ne =
            detail::make_binary(ExprKind::NotEqual, map_expr->key_var(i), map_expr->key_var(j));
        solver.add_constraint(ne);
      }
    }
  }

  std::vector<Expr*> pending_hard(payload.element_hard.begin(), payload.element_hard.end());
  while (!pending_hard.empty()) {
    Expr* c = pending_hard.back();
    pending_hard.pop_back();
    if (c->kind() == ExprKind::ForEachI || c->kind() == ExprKind::ForEachKV ||
        c->kind() == ExprKind::Unique) {
      auto expanded = expand_constraint(c, cc);
      pending_hard.insert(pending_hard.end(), expanded.begin(), expanded.end());
    } else {
      Expr* r = rewrite_array_aggs(c, cc);
      solver.add_constraint(*r);
    }
  }

  std::vector<Expr*> expanded_soft_constraints;
  std::vector<Expr*> pending_soft(payload.element_soft.begin(), payload.element_soft.end());
  while (!pending_soft.empty()) {
    Expr* c = pending_soft.back();
    pending_soft.pop_back();
    if (c->kind() == ExprKind::ForEachI || c->kind() == ExprKind::ForEachKV ||
        c->kind() == ExprKind::Unique) {
      auto expanded = expand_constraint(c, cc);
      pending_soft.insert(pending_soft.end(), expanded.begin(), expanded.end());
    } else {
      expanded_soft_constraints.push_back(rewrite_array_aggs(c, cc));
    }
  }
  shuffle_soft_constraints(expanded_soft_constraints, seed + 31);
  add_soft_constraints_greedily(solver, expanded_soft_constraints);

  if (!solver.solve()) {
    return false;
  }
  solver.write_back();
  return true;
}

bool SolveStrategy::schedule_finalize_maps(ConstraintCollector& cc) {
  for (auto* map_expr : cc.all_maps()) {
    const std::size_t expected = map_expr->concrete_size();
    map_expr->write_back_entries();
    if (map_expr->actual_size() != expected) {
      return false;
    }
  }
  return true;
}

}  // namespace fuzzy
