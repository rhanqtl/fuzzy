#pragma once

#include "Graph.h"

#include "fuzzy/ast/Expr.h"
#include "fuzzy/ast/Visitor.h"

namespace fuzzy {
namespace detail {
struct GraphBuilder : public ASTVisitor<GraphBuilder> {
 public:
  GraphBuilder(Graph& graph, std::span<Expr*> csts) :
      graph_{graph},
      csts_{csts} {}

 public:
  void Build() {
    for (Expr* cst : csts_) {
      TraverseExpr(*cst);
    }
  }

 private:
  Graph& graph_;
  std::span<Expr*> csts_;
};
}  // namespace detail

struct Scheduler {
  std::vector<std::vector<uint64_t>> Run(std::span<Expr*> csts) {
    Graph graph(-1);

    GraphBuilder builder{graph, csts};
    builder.Build();

    auto order = TopoSort(graph);
  }
};

}  // namespace fuzzy
