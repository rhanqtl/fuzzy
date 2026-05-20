#ifndef FUZZY_AST_RAND_STRING_H
#define FUZZY_AST_RAND_STRING_H

#include <cstddef>
#include <string>

#include "fuzzy/ast/Expr.h"
#include "fuzzy/ast/RandVar.h"

namespace fuzzy {
// Only ASCII strings
class RandString : public Expr {
 public:
  VarExpr<std::size_t>& length() {
    return len_var_;
  }

 private:
  VarExpr<std::size_t> len_var_{len_, "__str_len"};
  std::size_t len_{0};
  std::string data_;
};
}  // namespace fuzzy

#endif
