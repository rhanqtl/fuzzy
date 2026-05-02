#ifndef FUZZY_SOLVER_Z3_H
#define FUZZY_SOLVER_Z3_H

#include "z3++.h"

#include "fuzzy/Assert.h"
#include "fuzzy/ast/Context.h"
#include "fuzzy/ast/Expr.h"
#include "fuzzy/ast/RandVar.h"

namespace fuzzy {
class ToZ3Visitor {
 public:
  ToZ3Visitor(z3::context& ctx) :
      ctx_{ctx} {}

  void visit(Expr& expr) {
    if (expr.is<ExprKind::Constant>()) {
      handle_constant(expr);
    }
    else if (expr.is<ExprKind::RandVar>()) {
      handle_var(expr);
    }
    else {
      handle_expr(expr);
    }
  }

  void handle_constant(Expr& expr) {
    fuzzy_assert(expr.is<ExprKind::Constant>());
    switch (expr) {}
  }

 private:
  z3::context& ctx_;
};

class Z3Solver {
 public:
  Z3Solver() :
      solver_{ctx_} {}

 public:
  void from_fuzzy(std::vector<Expr*>& roots) {
    ToZ3Visitor vis{ctx_};
    for (auto* root : roots) {
      vis.visit(*root);
    }
  }

 private:
  z3::context ctx_;
  z3::solver solver_;
};
}  // namespace fuzzy

#endif  // FUZZY_SOLVER_Z3_H
