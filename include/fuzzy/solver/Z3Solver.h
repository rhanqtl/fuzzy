#ifndef FUZZY_SOLVER_Z3_SOLVER_H
#define FUZZY_SOLVER_Z3_SOLVER_H

#include <cassert>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "z3++.h"

#include "fuzzy/ast/Expr.h"
#include "fuzzy/ast/RandArray.h"
#include "fuzzy/ast/RandVar.h"

namespace fuzzy {

class Z3Solver {
 public:
  explicit Z3Solver(unsigned seed = 0) :
      solver_{ctx_} {
    z3::params p(ctx_);
    p.set("random_seed", seed);
    // p.set("phase", "random");
    solver_.set(p);
  }

  // Recursively translate a fuzzy Expr to z3::expr
  z3::expr translate(Expr& e) {
    switch (e.kind()) {
      case ExprKind::Var: {
        return get_or_create_var(e);
      }
      case ExprKind::Const: {
        auto& c = static_cast<ConstExpr&>(e);
        return ctx_.int_val(c.as_int());
      }
      case ExprKind::Add: {
        auto& b = static_cast<BinaryExpr&>(e);
        return translate(*b.lhs()) + translate(*b.rhs());
      }
      case ExprKind::Sub: {
        auto& b = static_cast<BinaryExpr&>(e);
        return translate(*b.lhs()) - translate(*b.rhs());
      }
      case ExprKind::Mul: {
        auto& b = static_cast<BinaryExpr&>(e);
        return translate(*b.lhs()) * translate(*b.rhs());
      }
      case ExprKind::Div: {
        auto& b = static_cast<BinaryExpr&>(e);
        return translate(*b.lhs()) / translate(*b.rhs());
      }
      case ExprKind::Mod: {
        auto& b = static_cast<BinaryExpr&>(e);
        return translate(*b.lhs()) % translate(*b.rhs());
      }
      case ExprKind::Abs: {
        auto& u = static_cast<UnaryExpr&>(e);
        auto val = translate(*u.operand());
        return z3::ite(val >= 0, val, -val);
      }
      case ExprKind::Less: {
        auto& b = static_cast<BinaryExpr&>(e);
        return translate(*b.lhs()) < translate(*b.rhs());
      }
      case ExprKind::LessEqual: {
        auto& b = static_cast<BinaryExpr&>(e);
        return translate(*b.lhs()) <= translate(*b.rhs());
      }
      case ExprKind::Greater: {
        auto& b = static_cast<BinaryExpr&>(e);
        return translate(*b.lhs()) > translate(*b.rhs());
      }
      case ExprKind::GreaterEqual: {
        auto& b = static_cast<BinaryExpr&>(e);
        return translate(*b.lhs()) >= translate(*b.rhs());
      }
      case ExprKind::Equal: {
        auto& b = static_cast<BinaryExpr&>(e);
        return translate(*b.lhs()) == translate(*b.rhs());
      }
      case ExprKind::NotEqual: {
        auto& b = static_cast<BinaryExpr&>(e);
        return translate(*b.lhs()) != translate(*b.rhs());
      }
      case ExprKind::LogicalAnd: {
        auto& b = static_cast<BinaryExpr&>(e);
        return translate(*b.lhs()) && translate(*b.rhs());
      }
      case ExprKind::LogicalOr: {
        auto& b = static_cast<BinaryExpr&>(e);
        return translate(*b.lhs()) || translate(*b.rhs());
      }
      case ExprKind::LogicalNot: {
        auto& u = static_cast<UnaryExpr&>(e);
        return !translate(*u.operand());
      }
      case ExprKind::Implies: {
        auto& imp = static_cast<ImpliesExpr&>(e);
        return z3::implies(translate(*imp.cond()), translate(*imp.body()));
      }
      case ExprKind::Ite: {
        auto& it = static_cast<IteExpr&>(e);
        return z3::ite(translate(*it.cond()), translate(*it.then_expr()), translate(*it.else_expr()));
      }
      case ExprKind::ArraySize: {
        // Translate to the size variable
        auto& as = static_cast<ArraySizeExpr&>(e);
        return get_or_create_var(as.array()->size_var_base());
      }
      case ExprKind::Call: {
        assert(false && "CallExpr must be reduced to ConstExpr before Z3 translation");
        return ctx_.int_val(0);
      }
      default:
        assert(false &&
               "Unexpected ExprKind in Z3 translation (ForEachI/ForEachKV/Unique/ArrayAgg/Array should be "
               "expanded)");
        return ctx_.bool_val(false);
    }
  }

  void add_constraint(Expr& e) {
    solver_.add(translate(e));
  }

  void add_constraints(const std::vector<Expr*>& exprs) {
    for (auto* e : exprs) {
      add_constraint(*e);
    }
  }

  bool solve() {
    return solver_.check() == z3::sat;
  }

  // Extract values from z3 model and write back to VarExpr out_ references
  void write_back() {
    auto model = solver_.get_model();
    for (auto& [var_ptr, z3var] : var_map_) {
      auto* var_base = static_cast<VarExprBase*>(var_ptr);
      auto val = model.eval(z3var, true);
      int64_t int_val = 0;
      if (val.is_numeral_i64(int_val)) {
        var_base->write_back(int_val);
      }
    }
  }

  z3::expr get_or_create_var(Expr& var_expr) {
    auto it = var_map_.find(&var_expr);
    if (it != var_map_.end()) {
      return it->second;
    }

    auto* var_base = static_cast<VarExprBase*>(&var_expr);
    auto z3var = ctx_.int_const(var_base->name().c_str());
    var_map_.emplace(&var_expr, z3var);
    return z3var;
  }

  z3::context& context() {
    return ctx_;
  }
  z3::solver& solver() {
    return solver_;
  }

 private:
  z3::context ctx_;
  z3::solver solver_;
  std::unordered_map<Expr*, z3::expr> var_map_;
};

}  // namespace fuzzy

#endif  // FUZZY_SOLVER_Z3_SOLVER_H
