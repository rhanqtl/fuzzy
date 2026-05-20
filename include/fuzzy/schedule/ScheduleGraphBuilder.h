#ifndef FUZZY_SCHEDULE_GRAPH_BUILDER_H
#define FUZZY_SCHEDULE_GRAPH_BUILDER_H

#include "fuzzy/ConstraintCollector.h"
#include "fuzzy/schedule/DependencyGraph.h"
#include "fuzzy/schedule/ScheduleTypes.h"

namespace fuzzy {

class SolveStrategy;

namespace schedule {

/// Classifies collected constraints and builds the macro scheduling DAG.
/// Finer per-var / per-call nodes can be added inside SolveScalar* phases later.
class ScheduleGraphBuilder {
 public:
  ScheduleGraphBuilder(SolveStrategy& strategy, const ConstraintCollector& cc,
                       SchedulePayload& payload, DependencyGraph& graph) :
      strategy_{strategy},
      cc_{cc},
      payload_{payload},
      graph_{graph} {}

  void build();

  [[nodiscard]] const SchedulePayload& payload() const {
    return payload_;
  }

 private:
  void collect_from_blocks();
  void classify_constraints();
  void emit_graph_nodes();

  SolveStrategy& strategy_;
  const ConstraintCollector& cc_;
  SchedulePayload& payload_;
  DependencyGraph& graph_;

  SchedNodeId pins_{kInvalidSchedNode};
  SchedNodeId scalar_no_call_{kInvalidSchedNode};
  SchedNodeId scalar_with_call_{kInvalidSchedNode};
  SchedNodeId commit_randc_{kInvalidSchedNode};
  SchedNodeId materialize_{kInvalidSchedNode};
  SchedNodeId elements_{kInvalidSchedNode};
};

}  // namespace schedule
}  // namespace fuzzy

#endif  // FUZZY_SCHEDULE_GRAPH_BUILDER_H
