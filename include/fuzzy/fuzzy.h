#ifndef FUZZY_FUZZY_H
#define FUZZY_FUZZY_H

#include <concepts>
#include <cstddef>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "fuzzy/ConstraintCollector.h"
#include "fuzzy/Model.h"
#include "fuzzy/ast/Expr.h"
#include "fuzzy/ast/Func.h"
#include "fuzzy/ast/RandArray.h"
#include "fuzzy/ast/RandVar.h"

// ============================================================
// PP_FOR_EACH — recursive macro expansion via __VA_OPT__
// Supports up to ~256 arguments (4 levels of expansion).
// ============================================================

#define _FUZZY_PARENS ()

#define _FUZZY_EXPAND(...) \
  _FUZZY_EXPAND4(_FUZZY_EXPAND4(_FUZZY_EXPAND4(_FUZZY_EXPAND4(__VA_ARGS__))))
#define _FUZZY_EXPAND4(...) \
  _FUZZY_EXPAND3(_FUZZY_EXPAND3(_FUZZY_EXPAND3(_FUZZY_EXPAND3(__VA_ARGS__))))
#define _FUZZY_EXPAND3(...) \
  _FUZZY_EXPAND2(_FUZZY_EXPAND2(_FUZZY_EXPAND2(_FUZZY_EXPAND2(__VA_ARGS__))))
#define _FUZZY_EXPAND2(...) \
  _FUZZY_EXPAND1(_FUZZY_EXPAND1(_FUZZY_EXPAND1(_FUZZY_EXPAND1(__VA_ARGS__))))
#define _FUZZY_EXPAND1(...) __VA_ARGS__

#define _FUZZY_FOR_EACH(macro, a, ...) \
  macro(a) __VA_OPT__(_FUZZY_FOR_EACH_AGAIN _FUZZY_PARENS(macro, __VA_ARGS__))
#define _FUZZY_FOR_EACH_AGAIN() _FUZZY_FOR_EACH

// Create a proxy variable that shadows the real field
#define _FUZZY_MKPROXY(field) auto& field = __cc.create_proxy(__obj.field, #field);

// ============================================================
// FUZZY_META / BLOCK / FUZZY_END — core DSL macros
// ============================================================

#define FUZZY_META(Class, ...)                                                                  \
  friend class ::fuzzy::Model<Class>;                                                           \
  static void __fuzzy_define(Class& __obj, ::fuzzy::ConstraintCollector& __cc) {                \
    _FUZZY_EXPAND(_FUZZY_FOR_EACH(_FUZZY_MKPROXY, __VA_ARGS__))                                 \
    [[maybe_unused]] auto constrain = [&](::fuzzy::Expr& e) { __cc.add_constraint(e); };        \
    [[maybe_unused]] auto for_each_i = [&](::fuzzy::ArrayExprBase& arr, auto&& fn) {            \
      __cc.for_each_i(arr, std::forward<decltype(fn)>(fn));                                     \
    };                                                                                          \
    [[maybe_unused]] auto unique = [&](::fuzzy::ArrayExprBase& arr) { __cc.add_unique(arr); };  \
    [[maybe_unused]] auto implies = [&](::fuzzy::Expr& c, ::fuzzy::Expr& b) -> ::fuzzy::Expr& { \
      return __cc.make_implies(c, b);                                                           \
    };                                                                                          \
    [[maybe_unused]] auto invoke = [&](auto& self, const auto& fn, auto&&... args)            \
        -> ::fuzzy::Expr& {                                                                     \
      return ::fuzzy::dsl::invoke(self, fn, std::forward<decltype(args)>(args)...);            \
    };                                                                                          \
    [[maybe_unused]] auto& abs = ::fuzzy::abs;

#define BLOCK(name)        \
  __cc.begin_block(#name); \
  if (true)

#define FUZZY_END  \
  __cc.finalize(); \
  }

#define FUZZY_DECLARE_FUNC(fn) ::fuzzy::dsl::declare_func(fn)

#define FUZZY_CALL(obj, fn, ...) ::fuzzy::dsl::invoke((obj), (fn)__VA_OPT__(, ) __VA_ARGS__)

#endif  // FUZZY_FUZZY_H
