#ifndef FUZZY_FUNC_H
#define FUZZY_FUNC_H

#include <concepts>
#include <cstdint>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "fuzzy/ast/Expr.h"

namespace fuzzy {

namespace dsl {

template <typename T>
concept DSLIntegral = std::integral<std::remove_cvref_t<T>>;

inline Expr& to_expr(Expr& e) {
  return e;
}

template <DSLIntegral T>
inline Expr& to_expr(T v) {
  return detail::make_const(static_cast<int64_t>(v));
}

template <typename FnPtr>
class DeclaredFunc;

template <typename Ret, typename... Args>
class DeclaredFunc<Ret (*)(Args...)> {
 public:
  using FnType = Ret (*)(Args...);

  explicit DeclaredFunc(FnType fn) :
      fn_{fn} {}

  template <typename Obj>
  int64_t call(Obj&, const std::vector<int64_t>& args) const {
    return call_impl(args, std::index_sequence_for<Args...>{});
  }

 private:
  template <std::size_t... I>
  int64_t call_impl(const std::vector<int64_t>& args, std::index_sequence<I...>) const {
    return static_cast<int64_t>(fn_(static_cast<Args>(args[I])...));
  }

  FnType fn_;
};

template <typename C, typename Ret, typename... Args>
class DeclaredFunc<Ret (C::*)(Args...)> {
 public:
  using FnType = Ret (C::*)(Args...);

  explicit DeclaredFunc(FnType fn) :
      fn_{fn} {}

  int64_t call(C& obj, const std::vector<int64_t>& args) const {
    return call_impl(obj, args, std::index_sequence_for<Args...>{});
  }

 private:
  template <std::size_t... I>
  int64_t call_impl(C& obj, const std::vector<int64_t>& args, std::index_sequence<I...>) const {
    return static_cast<int64_t>((obj.*fn_)(static_cast<Args>(args[I])...));
  }

  FnType fn_;
};

template <typename C, typename Ret, typename... Args>
class DeclaredFunc<Ret (C::*)(Args...) const> {
 public:
  using FnType = Ret (C::*)(Args...) const;

  explicit DeclaredFunc(FnType fn) :
      fn_{fn} {}

  int64_t call(const C& obj, const std::vector<int64_t>& args) const {
    return call_impl(obj, args, std::index_sequence_for<Args...>{});
  }

 private:
  template <std::size_t... I>
  int64_t call_impl(const C& obj, const std::vector<int64_t>& args,
                    std::index_sequence<I...>) const {
    return static_cast<int64_t>((obj.*fn_)(static_cast<Args>(args[I])...));
  }

  FnType fn_;
};

template <typename FnPtr>
DeclaredFunc<std::remove_cvref_t<FnPtr>> declare_func(FnPtr fn) {
  return DeclaredFunc<std::remove_cvref_t<FnPtr>>(fn);
}

template <typename Obj, typename Func, typename... CallArgs>
Expr& invoke(Obj& obj, const Func& fn, CallArgs&&... call_args) {
  std::vector<Expr*> args = {&to_expr(std::forward<CallArgs>(call_args))...};
  auto eval_fn = [&obj, fn](const std::vector<int64_t>& concrete_args) -> int64_t {
    return fn.call(obj, concrete_args);
  };
  return detail::make_call(std::move(args), std::move(eval_fn));
}

}  // namespace dsl
}  // namespace fuzzy
#endif  // FUZZY_FUNC_H
