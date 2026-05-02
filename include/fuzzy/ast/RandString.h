#ifndef FUZZY_AST_RAND_STRING_H
#define FUZZY_AST_RAND_STRING_H

#include <cstddef>
#include <functional>
#include <memory>

#include "fuzzy/ast/Expr.h"
#include "fuzzy/ast/RandVar.h"

namespace fuzzy {
// Only ASCII strings
class RandString : public Expr {
 public:
  RandVar<std::size_t>& length() {
    return len_var_;
  }

 private:
  RandVar<std::size_t> len_var_;
  std::size_t len_{0};
  std::string data_;
};
}  // namespace fuzzy

#endif
