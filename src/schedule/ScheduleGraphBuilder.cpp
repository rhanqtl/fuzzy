#include "fuzzy/schedule/ScheduleGraphBuilder.h"

#include "fuzzy/SolveStrategy.h"

namespace fuzzy::schedule {

void ScheduleGraphBuilder::build() {
  collect_from_blocks();
  classify_constraints();
  emit_graph_nodes();
}

void ScheduleGraphBuilder::collect_from_blocks() {
  auto& blocks = cc_.blocks();
  for (std::size_t block_idx = 0; block_idx < blocks.size(); ++block_idx) {
    const auto& block = blocks[block_idx];
    if (strategy_.block_is_overridden(blocks, block_idx)) {
      continue;
    }
    for (Expr* c : block.constraints) {
      payload_.collected.hard.push_back(c);
    }
    for (Expr* c : block.soft_constraints) {
      payload_.collected.soft.push_back(c);
    }
    for (Expr* c : block.disabled_soft_constraints) {
      payload_.collected.disabled_soft.push_back(c);
    }
    for (const auto& d : block.dist_constraints) {
      payload_.collected.dist.push_back(d);
    }
  }

  if (!payload_.collected.disabled_soft.empty()) {
    strategy_.disabled_soft_constraints_.insert(
        strategy_.disabled_soft_constraints_.end(),
        payload_.collected.disabled_soft.begin(), payload_.collected.disabled_soft.end());
    std::vector<Expr*> filtered;
    filtered.reserve(payload_.collected.soft.size());
    for (Expr* c : payload_.collected.soft) {
      if (!strategy_.soft_is_disabled(c)) {
        filtered.push_back(c);
      }
    }
    payload_.collected.soft = std::move(filtered);
  }
}

void ScheduleGraphBuilder::classify_constraints() {
  std::vector<Expr*> scalar_hard_all;
  std::vector<Expr*> scalar_soft_all;

  for (Expr* c : payload_.collected.hard) {
    if (strategy_.involves_array_elements(c)) {
      payload_.element_hard.push_back(c);
    } else {
      scalar_hard_all.push_back(c);
    }
  }
  for (Expr* c : payload_.collected.soft) {
    if (strategy_.involves_array_elements(c)) {
      payload_.element_soft.push_back(c);
    } else {
      scalar_soft_all.push_back(c);
    }
  }

  for (Expr* c : scalar_hard_all) {
    if (strategy_.contains_call(c)) {
      payload_.scalar_hard_with_call.push_back(c);
    } else {
      payload_.scalar_hard_no_call.push_back(c);
    }
  }
  for (Expr* c : scalar_soft_all) {
    if (strategy_.contains_call(c)) {
      payload_.scalar_soft_with_call.push_back(c);
    } else {
      payload_.scalar_soft_no_call.push_back(c);
    }
  }

  for (auto& d : payload_.collected.dist) {
    if (d.expr && !strategy_.contains_call(d.expr) && !strategy_.involves_array_elements(d.expr)) {
      payload_.scalar_dist.push_back(std::move(d));
    }
  }

  payload_.has_scalar_call_phase = !payload_.scalar_hard_with_call.empty();
}

void ScheduleGraphBuilder::emit_graph_nodes() {
  pins_ = graph_.add_node(SchedNodeKind::ApplyPins);
  scalar_no_call_ = graph_.add_node(SchedNodeKind::SolveScalarNoCall);
  graph_.add_edge(pins_, scalar_no_call_);

  SchedNodeId last_scalar = scalar_no_call_;
  if (payload_.has_scalar_call_phase) {
    scalar_with_call_ = graph_.add_node(SchedNodeKind::SolveScalarWithCall);
    graph_.add_edge(scalar_no_call_, scalar_with_call_);
    last_scalar = scalar_with_call_;
  }

  commit_randc_ = graph_.add_node(SchedNodeKind::CommitRandc);
  graph_.add_edge(last_scalar, commit_randc_);

  materialize_ = graph_.add_node(SchedNodeKind::MaterializeContainers);
  graph_.add_edge(commit_randc_, materialize_);

  elements_ = graph_.add_node(SchedNodeKind::SolveElements);
  graph_.add_edge(materialize_, elements_);
}

}  // namespace fuzzy::schedule
