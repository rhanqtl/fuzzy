#ifndef FUZZY_SCHEDULE_ENGINE_H
#define FUZZY_SCHEDULE_ENGINE_H

#include "fuzzy/ConstraintCollector.h"
#include "fuzzy/schedule/DependencyGraph.h"
#include "fuzzy/schedule/ScheduleGraphBuilder.h"
#include "fuzzy/schedule/ScheduleTypes.h"

namespace fuzzy {

class SolveStrategy;

namespace schedule {

/// Graph-driven scheduler: repeatedly executes zero-dependency nodes until done.
class ScheduleEngine {
 public:
  explicit ScheduleEngine(SolveStrategy& strategy) :
      strategy_{strategy} {}

  bool run(ConstraintCollector& cc, unsigned seed);

 private:
  bool execute_node(SchedNodeKind kind, ConstraintCollector& cc, unsigned seed,
                    const SchedulePayload& payload);

  SolveStrategy& strategy_;
};

}  // namespace schedule
}  // namespace fuzzy

#endif  // FUZZY_SCHEDULE_ENGINE_H
