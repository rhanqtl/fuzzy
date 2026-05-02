#ifndef FUZZY_FUNC_H
#define FUZZY_FUNC_H

#include <functional>

#include "fuzzy/ast/Constant.h"
#include "fuzzy/ast/Expr.h"
#include "fuzzy/ast/RandVar.h"

namespace fuzzy {
template <typename Ret, typename... Args>
class Func;

template <typename Ret, typename... Args>
class Func<Ret(Args...)> {
 public:
  template <typename F>
  Func(F&& real_fn);

 public:
  Expr& operator()(RandVar<Args>&... args);

 private:
  std::function<Ret(Args...)> real_fn_;
};
}  // namespace fuzzy
#endif  // FUZZY_FUNC_H
