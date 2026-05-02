#pragma once

#include "fuzzy/ast/Expr.h"

namespace fuzzy {
class Var;

class ASTVisitor {
 public:
  void TraverseExpr(Expr& expr) {
    switch (expr.kind()) {
      case ExprKind::Var:
        TraverseVar(static_cast<Var&>(expr));
        break;
      // case ExprKind::Add:
      //   TraverseAdd(static_cast<Add&>(expr));
      //   break;
      default:
        fuzzy_unreachable("Unhandled ExprKind {}", static_cast<int>(expr.kind()));
    }
  }

  void TraverseVar(Var& var) {
    derived().VisitVar(var);
  }

 private:
  DerivedT& derived() {
    return *static_cast<DerivedT*>(this);
  }
};
}  // namespace fuzzy
