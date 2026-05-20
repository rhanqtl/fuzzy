#ifndef FUZZY_SCHEDULE_DEPENDENCY_GRAPH_H
#define FUZZY_SCHEDULE_DEPENDENCY_GRAPH_H

#include <cstddef>
#include <stdexcept>
#include <vector>

#include "fuzzy/schedule/ScheduleTypes.h"

namespace fuzzy::schedule {

/// Directed acyclic scheduling graph. Edge `pred -> succ` means `succ` waits for `pred`.
class DependencyGraph {
 public:
  SchedNodeId add_node(SchedNodeKind kind) {
    const SchedNodeId id = nodes_.size();
    nodes_.push_back(Node{kind, 0, false});
    incoming_.emplace_back();
    outgoing_.emplace_back();
    return id;
  }

  void add_edge(SchedNodeId pred, SchedNodeId succ) {
    if (pred >= nodes_.size() || succ >= nodes_.size()) {
      throw std::out_of_range("DependencyGraph::add_edge: invalid node id");
    }
    outgoing_[pred].push_back(succ);
    incoming_[succ].push_back(pred);
    nodes_[succ].pending_deps++;
  }

  [[nodiscard]] std::vector<SchedNodeId> ready_nodes() const {
    std::vector<SchedNodeId> ready;
    ready.reserve(nodes_.size());
    for (SchedNodeId id = 0; id < nodes_.size(); ++id) {
      if (!nodes_[id].done && nodes_[id].pending_deps == 0) {
        ready.push_back(id);
      }
    }
    return ready;
  }

  void mark_done(SchedNodeId id) {
    if (id >= nodes_.size() || nodes_[id].done) {
      return;
    }
    nodes_[id].done = true;
    for (SchedNodeId succ : outgoing_[id]) {
      if (nodes_[succ].pending_deps > 0) {
        nodes_[succ].pending_deps--;
      }
    }
  }

  [[nodiscard]] bool all_done() const {
    for (const Node& n : nodes_) {
      if (!n.done) {
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] bool has_pending() const {
    return !all_done();
  }

  [[nodiscard]] SchedNodeKind kind(SchedNodeId id) const {
    return nodes_.at(id).kind;
  }

  [[nodiscard]] std::size_t size() const {
    return nodes_.size();
  }

 private:
  struct Node {
    SchedNodeKind kind;
    std::size_t pending_deps;
    bool done;
  };

  std::vector<Node> nodes_;
  std::vector<std::vector<SchedNodeId>> incoming_;
  std::vector<std::vector<SchedNodeId>> outgoing_;
};

}  // namespace fuzzy::schedule

#endif  // FUZZY_SCHEDULE_DEPENDENCY_GRAPH_H
