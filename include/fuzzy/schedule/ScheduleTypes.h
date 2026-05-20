#ifndef FUZZY_SCHEDULE_TYPES_H
#define FUZZY_SCHEDULE_TYPES_H

#include <cstddef>
#include <vector>

#include "fuzzy/ConstraintCollector.h"
#include "fuzzy/ast/Expr.h"

namespace fuzzy::schedule {

using SchedNodeId = std::size_t;
inline constexpr SchedNodeId kInvalidSchedNode = static_cast<SchedNodeId>(-1);

/// Kind of scheduling unit. Unidirectional edges are emitted only between kinds
/// that represent ordering requirements (call args, solve_before, size→materialize).
enum class SchedNodeKind {
  /// Pin non-decision variables to current values (L0 decision set already applied).
  ApplyPins,
  /// Scalar hard constraints without CallExpr; includes array/map size vars.
  SolveScalarNoCall,
  /// Reduce CallExpr subtrees and solve remaining scalar hard constraints.
  SolveScalarWithCall,
  /// Record randc draws after scalar phases.
  CommitRandc,
  /// Resize arrays/maps and create element/key/value decision vars.
  MaterializeContainers,
  /// Expand foreach/unique/aggs and solve element-level constraints.
  SolveElements,
};

struct CollectedConstraints {
  std::vector<Expr*> hard;
  std::vector<Expr*> soft;
  std::vector<Expr*> disabled_soft;
  std::vector<ConstraintCollector::DistConstraint> dist;
};

struct SchedulePayload {
  CollectedConstraints collected;
  std::vector<Expr*> scalar_hard_no_call;
  std::vector<Expr*> scalar_hard_with_call;
  std::vector<Expr*> scalar_soft_no_call;
  std::vector<Expr*> scalar_soft_with_call;
  std::vector<Expr*> element_hard;
  std::vector<Expr*> element_soft;
  std::vector<ConstraintCollector::DistConstraint> scalar_dist;
  bool has_scalar_call_phase{false};
};

}  // namespace fuzzy::schedule

#endif  // FUZZY_SCHEDULE_TYPES_H
