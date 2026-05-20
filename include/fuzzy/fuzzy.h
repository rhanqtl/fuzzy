#ifndef FUZZY_FUZZY_H
#define FUZZY_FUZZY_H

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "fuzzy/ConstraintCollector.h"
#include "fuzzy/Dict.h"
#include "fuzzy/Model.h"
#include "fuzzy/RandMember.h"
#include "fuzzy/ScopedRandomize.h"
#include "fuzzy/ast/Expr.h"
#include "fuzzy/ast/Func.h"
#include "fuzzy/ast/RandArray.h"
#include "fuzzy/ast/RandVar.h"
#include "fuzzy/dsl/Context.h"

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

#define _FUZZY_FOR_EACH_CTX(macro, C, a, ...) \
  macro(C, a) __VA_OPT__(_FUZZY_FOR_EACH_CTX_AGAIN _FUZZY_PARENS(macro, C, __VA_ARGS__))
#define _FUZZY_FOR_EACH_CTX_AGAIN() _FUZZY_FOR_EACH_CTX

#define _FUZZY_META_CAT(a, b) _FUZZY_META_CAT_I(a, b)
#define _FUZZY_META_CAT_I(a, b) a##b

// ---- is_begin_parens (Boost.Preprocessor punctuation/detail/is_begin_parens.hpp logic) ----
#define _FUZZY_IBP_PRIMITIVE_CAT(a, ...) a##__VA_ARGS__
#define _FUZZY_IBP_CAT(a, ...) _FUZZY_IBP_PRIMITIVE_CAT(a, __VA_ARGS__)
#define _FUZZY_IBP_SPLIT(i, ...) _FUZZY_IBP_PRIMITIVE_CAT(_FUZZY_IBP_SPLIT_, i)(__VA_ARGS__)
#define _FUZZY_IBP_SPLIT_0(a, ...) a
#define _FUZZY_IBP_SPLIT_1(a, ...) __VA_ARGS__
#define _FUZZY_IBP_IS_VARIADIC_R_1 1,
#define _FUZZY_IBP_IS_VARIADIC_R__FUZZY_IBP_IS_VARIADIC_C 0,
#define _FUZZY_IBP_IS_VARIADIC_C(...) 1
#define _FUZZY_IBP_IS_BEGIN_PARENS(...) \
  _FUZZY_IBP_SPLIT(0, _FUZZY_IBP_CAT(_FUZZY_IBP_IS_VARIADIC_R_, _FUZZY_IBP_IS_VARIADIC_C __VA_ARGS__))

// ---- inheritance helpers (bases is `void` or a parenthesized list (B1, B2, ...)) ----

#define _FUZZY_APPLY_INHERITS(tuple) _FUZZY_INHERITS tuple
#define _FUZZY_INHERITS(b1, ...) public b1::__FuzzyDefiner __VA_OPT__(, _FUZZY_INHERITS2(__VA_ARGS__))
#define _FUZZY_INHERITS2(b1, ...) public b1::__FuzzyDefiner __VA_OPT__(, _FUZZY_INHERITS(__VA_ARGS__))

#define _FUZZY_BASE_CTORS_LEAD(tuple) _FUZZY_BASE_CTORS_LEAD2 tuple
#define _FUZZY_BASE_CTORS_LEAD2(...) \
  __VA_OPT__(__FUZZY_BASE_CTORS_BODY(__VA_ARGS__)) __VA_OPT__(,)
#define __FUZZY_BASE_CTORS_BODY(b1, ...) \
  b1::__FuzzyDefiner(static_cast<b1&>(__obj_arg), __cc_arg) __VA_OPT__(, __FUZZY_BASE_CTORS_BODY2(__VA_ARGS__))
#define __FUZZY_BASE_CTORS_BODY2(b1, ...) \
  b1::__FuzzyDefiner(static_cast<b1&>(__obj_arg), __cc_arg) __VA_OPT__(, __FUZZY_BASE_CTORS_BODY(__VA_ARGS__))

#define _FUZZY_APPLY_BASE_DEFINES(tuple) _FUZZY_BASE_DEFINES tuple
#define _FUZZY_BASE_DEFINES(...) __VA_OPT__(__FUZZY_BASE_DEFINES_BODY(__VA_ARGS__))
#define __FUZZY_BASE_DEFINES_BODY(b1, ...) \
  b1::__FuzzyDefiner::define(); __VA_OPT__(__FUZZY_BASE_DEFINES_BODY2(__VA_ARGS__))
#define __FUZZY_BASE_DEFINES_BODY2(b1, ...) \
  b1::__FuzzyDefiner::define(); __VA_OPT__(__FUZZY_BASE_DEFINES_BODY(__VA_ARGS__))

// ---- FUZZY_META field specs: FUZZY_RAND(x), FUZZY_RANDC(y), or bare z (state) ----
// Internal tuple kinds use _FK_* tokens so we do not clash with Catch2's `RAND` macro.

#define FUZZY_RAND(x) (_FUZZY_FL, _FK_RAND, x)
#define FUZZY_RANDC(x) (_FUZZY_FL, _FK_RANDC, x)
/// Like `FUZZY_RAND(x)` but `x` is `fuzzy::RandMember<T>`; randomness flag lives on `x.rand_mode(...)`.
#define FUZZY_RANDM(x) (_FUZZY_FL, _FK_RANDM, x)

#define _FUZZY_FIELD_TUPLE_FROM(item) \
  _FUZZY_META_CAT(_FUZZY_FIELD_TUPLE_, _FUZZY_IBP_IS_BEGIN_PARENS(item))(item)
#define _FUZZY_FIELD_TUPLE_1(item) item
#define _FUZZY_FIELD_TUPLE_0(item) (_FUZZY_FL, STATE, item)

#define _FUZZY_FIELD_EXPAND(...) __VA_ARGS__
#define _FUZZY_APPLY(f, ...) f(__VA_ARGS__)

#define _FUZZY_COMPILE_KIND(kind) _FUZZY_META_CAT(_FUZZY_COMPILE_KIND_, kind)
#define _FUZZY_COMPILE_KIND__FK_RAND ::fuzzy::CompileRandKind::Rand
#define _FUZZY_COMPILE_KIND__FK_RANDC ::fuzzy::CompileRandKind::Randc
#define _FUZZY_COMPILE_KIND_STATE ::fuzzy::CompileRandKind::State
#define _FUZZY_COMPILE_KIND__FK_RANDM ::fuzzy::CompileRandKind::Rand

#define _FUZZY_RAND_MODE_PTR(kind, name) _FUZZY_META_CAT(_FUZZY_RAND_MODE_PTR_, kind)(name)
#define _FUZZY_RAND_MODE_PTR__FK_RAND(name) &__obj_arg.__fuzzy_rm_##name
#define _FUZZY_RAND_MODE_PTR__FK_RANDC(name) &__obj_arg.__fuzzy_rm_##name
#define _FUZZY_RAND_MODE_PTR_STATE(name) nullptr
#define _FUZZY_RAND_MODE_PTR__FK_RANDM(name) nullptr

#define _FUZZY_MODE_FLAG(C, item) _FUZZY_MODE_FLAG4(C, _FUZZY_FIELD_TUPLE_FROM(item))
#define _FUZZY_MODE_FLAG4(C, tuple) _FUZZY_APPLY(_FUZZY_MODE_FLAG_IMPL, C, _FUZZY_FIELD_EXPAND tuple)
#define _FUZZY_MODE_FLAG_IMPL(C, _FUZZY_FL, kind, name) _FUZZY_META_CAT(_FUZZY_MODE_EMIT_, kind)(name)
#define _FUZZY_MODE_EMIT__FK_RAND(name) bool __fuzzy_rm_##name = true;
#define _FUZZY_MODE_EMIT__FK_RANDC(name) \
  bool __fuzzy_rm_##name = true; \
  ::fuzzy::RandcCycle __fuzzy_randc_##name{};
#define _FUZZY_MODE_EMIT_STATE(name)
#define _FUZZY_MODE_EMIT__FK_RANDM(name)

// ---- per-field members in __FuzzyDefiner (C = enclosing class type) ----

#define _FUZZY_FIELD_MEMBER(C, item) _FUZZY_FIELD_MEMBER4(C, _FUZZY_FIELD_TUPLE_FROM(item))
#define _FUZZY_FIELD_MEMBER4(C, tuple) _FUZZY_APPLY(_FUZZY_FIELD_MEMBER4_IMPL, C, _FUZZY_FIELD_EXPAND tuple)
#define _FUZZY_FIELD_MEMBER4_IMPL(C, _FUZZY_FL, kind, name) \
  _FUZZY_META_CAT(_FUZZY_FIELD_MEMBER_, kind)(C, name)

#define _FUZZY_FIELD_MEMBER__FK_RAND(C, name) _FUZZY_FIELD_MEMBER_SCALAR(C, name, _FK_RAND)
#define _FUZZY_FIELD_MEMBER__FK_RANDC(C, name)                                                    \
  decltype(std::declval<::fuzzy::ConstraintCollector&>().create_proxy(                           \
      std::declval<decltype(C::name)&>(), #name, ::fuzzy::CompileRandKind::Randc,                \
      static_cast<bool*>(nullptr), static_cast<::fuzzy::RandcCycle*>(nullptr))) name;
#define _FUZZY_FIELD_MEMBER_STATE(C, name) _FUZZY_FIELD_MEMBER_SCALAR(C, name, STATE)
#define _FUZZY_FIELD_MEMBER__FK_RANDM(C, name)                                                    \
  decltype(std::declval<::fuzzy::ConstraintCollector&>().create_proxy(                           \
      std::declval<decltype(C::name)&>(), "", _FUZZY_COMPILE_KIND__FK_RANDM)) name;

#define _FUZZY_FIELD_MEMBER_SCALAR(C, name, kind)                                              \
  decltype(std::declval<::fuzzy::ConstraintCollector&>().create_proxy(                           \
      std::declval<decltype(C::name)&>(), #name, _FUZZY_COMPILE_KIND(kind),                     \
      static_cast<bool*>(nullptr)))                                                            \
      name;

#define _FUZZY_FIELD_INIT(C, item) _FUZZY_FIELD_INIT4(C, _FUZZY_FIELD_TUPLE_FROM(item))
#define _FUZZY_FIELD_INIT4(C, tuple) _FUZZY_APPLY(_FUZZY_FIELD_INIT4_IMPL, C, _FUZZY_FIELD_EXPAND tuple)
#define _FUZZY_FIELD_INIT4_IMPL(C, _FUZZY_FL, kind, name) \
  _FUZZY_META_CAT(_FUZZY_FIELD_INIT_, kind)(__cc_arg, __obj_arg, name)

#define _FUZZY_FIELD_INIT__FK_RAND(__cc_arg, __obj_arg, name)                                   \
  , name(__cc_arg.create_proxy(__obj_arg.name, #name, _FUZZY_COMPILE_KIND(_FK_RAND),             \
                               &__obj_arg.__fuzzy_rm_##name))
#define _FUZZY_FIELD_INIT__FK_RANDC(__cc_arg, __obj_arg, name)                                 \
  , name(__cc_arg.create_proxy(__obj_arg.name, #name, _FUZZY_COMPILE_KIND(_FK_RANDC),            \
                               &__obj_arg.__fuzzy_rm_##name, &__obj_arg.__fuzzy_randc_##name))
#define _FUZZY_FIELD_INIT_STATE(__cc_arg, __obj_arg, name)                                     \
  , name(__cc_arg.create_proxy(__obj_arg.name, #name, _FUZZY_COMPILE_KIND(STATE), nullptr))
#define _FUZZY_FIELD_INIT__FK_RANDM(__cc_arg, __obj_arg, name)                                 \
  , name(__cc_arg.create_proxy(__obj_arg.name, #name, _FUZZY_COMPILE_KIND(_FK_RANDM)))

// DSL locals delegate to DslContext (see fuzzy/dsl/Context.h).
#define _FUZZY_DSL_BIND(cc_ref)                                                                  \
  [[maybe_unused]] ::fuzzy::dsl::DslContext __dsl_ctx{cc_ref};                                   \
  [[maybe_unused]] auto require = [&](auto&& e) {                                                \
    __dsl_ctx.require(std::forward<decltype(e)>(e));                                             \
  };                                                                                             \
  [[maybe_unused]] auto for_each_i = [&](auto& arr, auto&& fn) {                               \
    __dsl_ctx.for_each_i(arr, std::forward<decltype(fn)>(fn));                                 \
  };                                                                                             \
  [[maybe_unused]] auto for_each_kv = [&](auto& m, auto&& fn) {                                 \
    __dsl_ctx.for_each_kv(m, std::forward<decltype(fn)>(fn));                                   \
  };                                                                                             \
  [[maybe_unused]] auto unique = [&](auto& first, auto&... rest) {                             \
    __dsl_ctx.unique(first, rest...);                                                           \
  };                                                                                             \
  [[maybe_unused]] auto soft = [&](::fuzzy::Expr& e) { __dsl_ctx.soft(e); };                    \
  [[maybe_unused]] auto disable_soft = [&](::fuzzy::Expr& e) { __dsl_ctx.disable_soft(e); };    \
  [[maybe_unused]] auto solve_before = [&](::fuzzy::Expr& lhs, ::fuzzy::Expr& rhs) {          \
    __dsl_ctx.solve_before(lhs, rhs);                                                           \
  };                                                                                             \
  [[maybe_unused]] auto dist = [&](auto& expr,                                                 \
                                    std::initializer_list<std::pair<int64_t, int>> values) {    \
    __dsl_ctx.dist(expr, values);                                                               \
  };                                                                                             \
  [[maybe_unused]] auto implies = [&](::fuzzy::Expr& c, ::fuzzy::Expr& b) -> ::fuzzy::Expr& {  \
    return __dsl_ctx.implies(c, b);                                                            \
  };                                                                                             \
  [[maybe_unused]] auto constraint_if = [&](::fuzzy::Expr& c, auto&& fn) {                      \
    __dsl_ctx.constraint_if(c, std::forward<decltype(fn)>(fn));                                \
  };                                                                                             \
  [[maybe_unused]] auto constraint_if_else = [&](::fuzzy::Expr& c, auto&& t_fn, auto&& e_fn) { \
    __dsl_ctx.constraint_if_else(c, std::forward<decltype(t_fn)>(t_fn),                        \
                                 std::forward<decltype(e_fn)>(e_fn));                          \
  };                                                                                             \
  [[maybe_unused]] auto inside = [&](::fuzzy::Expr& x, auto&&... specs) {                       \
    __dsl_ctx.inside(x, std::forward<decltype(specs)>(specs)...);                             \
  };                                                                                             \
  [[maybe_unused]] auto array_sum = [&](auto& arr, auto&&... with_fn) -> ::fuzzy::Expr& {       \
    return __dsl_ctx.array_sum(arr, std::forward<decltype(with_fn)>(with_fn)...);              \
  };                                                                                             \
  [[maybe_unused]] auto array_product = [&](auto& arr, auto&&... with_fn) -> ::fuzzy::Expr& {   \
    return __dsl_ctx.array_product(arr, std::forward<decltype(with_fn)>(with_fn)...);          \
  };                                                                                             \
  [[maybe_unused]] auto array_and = [&](auto& arr, auto&&... with_fn) -> ::fuzzy::Expr& {       \
    return __dsl_ctx.array_and(arr, std::forward<decltype(with_fn)>(with_fn)...);              \
  };                                                                                             \
  [[maybe_unused]] auto array_or = [&](auto& arr, auto&&... with_fn) -> ::fuzzy::Expr& {        \
    return __dsl_ctx.array_or(arr, std::forward<decltype(with_fn)>(with_fn)...);               \
  };                                                                                             \
  [[maybe_unused]] auto array_xor = [&](auto& arr, auto&&... with_fn) -> ::fuzzy::Expr& {       \
    return __dsl_ctx.array_xor(arr, std::forward<decltype(with_fn)>(with_fn)...);              \
  };                                                                                             \
  [[maybe_unused]] auto array_min = [&](auto& arr, auto&&... with_fn) -> ::fuzzy::Expr& {       \
    return __dsl_ctx.array_min(arr, std::forward<decltype(with_fn)>(with_fn)...);              \
  };                                                                                             \
  [[maybe_unused]] auto array_max = [&](auto& arr, auto&&... with_fn) -> ::fuzzy::Expr& {       \
    return __dsl_ctx.array_max(arr, std::forward<decltype(with_fn)>(with_fn)...);              \
  };                                                                                             \
  [[maybe_unused]] auto invoke = [&](auto& self, const auto& fn, auto&&... args)                \
      -> ::fuzzy::Expr& {                                                                       \
    return __dsl_ctx.invoke(self, fn, std::forward<decltype(args)>(args)...);                  \
  };                                                                                             \
  [[maybe_unused]] auto bit_select = [&](::fuzzy::Expr& x, unsigned bit) -> ::fuzzy::Expr& {   \
    return __dsl_ctx.bit_select(x, bit);                                                       \
  };                                                                                             \
  [[maybe_unused]] auto bit_slice = [&](::fuzzy::Expr& x, unsigned hi, unsigned lo)             \
      -> ::fuzzy::Expr& {                                                                       \
    return __dsl_ctx.bit_slice(x, hi, lo);                                                     \
  };                                                                                             \
  [[maybe_unused]] auto bit_concat = [&](::fuzzy::Expr& hi, unsigned lo_width,                 \
                                        ::fuzzy::Expr& lo) -> ::fuzzy::Expr& {                 \
    return __dsl_ctx.bit_concat(hi, lo_width, lo);                                             \
  };                                                                                             \
  [[maybe_unused]] auto wildcard_eq = [&](::fuzzy::Expr& value, int64_t pattern,               \
                                         int64_t care_mask) -> ::fuzzy::Expr& {                \
    return __dsl_ctx.wildcard_eq(value, pattern, care_mask);                                   \
  };                                                                                             \
  [[maybe_unused]] auto wildcard_ne = [&](::fuzzy::Expr& value, int64_t pattern,               \
                                         int64_t care_mask) -> ::fuzzy::Expr& {                \
    return __dsl_ctx.wildcard_ne(value, pattern, care_mask);                                   \
  };                                                                                             \
  [[maybe_unused]] auto& abs = ::fuzzy::abs;

// ============================================================
// FUZZY_META / BLOCK / FUZZY_END — core DSL macros
// ============================================================

// FUZZY_META(Class, bases, field_specs...)
//   field_specs: FUZZY_RAND(x), FUZZY_RANDC(y), bare z (state), or FUZZY_RANDM(m) for RandMember fields.
//   bases: `void` if no inheritance, otherwise `(Base1, Base2, ...)` for __FuzzyDefiner merge.
// Distinguish `void` vs `(B,...)` without `##` onto `(` (GCC rejects); use is_begin_parens + NV_void.
#define _FUZZY_META_KIND_FROM_BASES(bases) \
  _FUZZY_META_CAT(_FUZZY_META_KIND_IBP_, _FUZZY_IBP_IS_BEGIN_PARENS(bases))(bases)
#define _FUZZY_META_KIND_IBP_1(bases) TUPLE
#define _FUZZY_META_KIND_IBP_0(bases) _FUZZY_META_CAT(_FUZZY_META_KIND_NV_, bases)
#define _FUZZY_META_KIND_NV_void VOID
#define _FUZZY_META_VOID_IMPL(Class, bases, ...) _FUZZY_META_DISPATCH_0(Class, __VA_ARGS__)
#define _FUZZY_META_TUPLE_IMPL(Class, bases, ...) _FUZZY_META_DISPATCH_1(Class, bases, __VA_ARGS__)
#define FUZZY_META(Class, bases, ...) \
  _FUZZY_META_CAT(_FUZZY_META_CAT(_FUZZY_META_, _FUZZY_META_KIND_FROM_BASES(bases)), _IMPL)(Class, bases, __VA_ARGS__)

#define _FUZZY_META_DISPATCH_0(Class, ...) \
  friend class ::fuzzy::Model<Class>;                                                           \
  using __FuzzyClass = Class;                                                                   \
  mutable std::unordered_map<std::string, bool> __fuzzy_constraint_mode_;                       \
  bool __fuzzy_constraint_enabled(const char* __fuzzy_bn) const {                               \
    const auto __fuzzy_it = __fuzzy_constraint_mode_.find(__fuzzy_bn);                          \
    return __fuzzy_it == __fuzzy_constraint_mode_.end() || __fuzzy_it->second;                \
  }                                                                                             \
  void fuzzy_constraint_mode(const std::string& __fuzzy_nm, int __fuzzy_on) {                   \
    __fuzzy_constraint_mode_[__fuzzy_nm] = (__fuzzy_on != 0);                                   \
  }                                                                                             \
  _FUZZY_EXPAND(_FUZZY_FOR_EACH_CTX(_FUZZY_MODE_FLAG, Class, __VA_ARGS__))                       \
  struct __FuzzyDefiner {                                                                       \
    Class& __obj;                                                                               \
    ::fuzzy::ConstraintCollector& __cc;                                                         \
    _FUZZY_EXPAND(_FUZZY_FOR_EACH_CTX(_FUZZY_FIELD_MEMBER, Class, __VA_ARGS__))                   \
    explicit __FuzzyDefiner(Class& __obj_arg, ::fuzzy::ConstraintCollector& __cc_arg)             \
        : __obj(__obj_arg), __cc(__cc_arg) _FUZZY_EXPAND(                                       \
            _FUZZY_FOR_EACH_CTX(_FUZZY_FIELD_INIT, Class, __VA_ARGS__)) {}                       \
    void define() {                                                                             \
      _FUZZY_DSL_BIND(__cc)

#define _FUZZY_META_DISPATCH_1(Class, bases_tuple, ...) \
  friend class ::fuzzy::Model<Class>;                                                           \
  using __FuzzyClass = Class;                                                                   \
  mutable std::unordered_map<std::string, bool> __fuzzy_constraint_mode_;                       \
  bool __fuzzy_constraint_enabled(const char* __fuzzy_bn) const {                               \
    const auto __fuzzy_it = __fuzzy_constraint_mode_.find(__fuzzy_bn);                          \
    return __fuzzy_it == __fuzzy_constraint_mode_.end() || __fuzzy_it->second;                \
  }                                                                                             \
  void fuzzy_constraint_mode(const std::string& __fuzzy_nm, int __fuzzy_on) {                   \
    __fuzzy_constraint_mode_[__fuzzy_nm] = (__fuzzy_on != 0);                                   \
  }                                                                                             \
  _FUZZY_EXPAND(_FUZZY_FOR_EACH_CTX(_FUZZY_MODE_FLAG, Class, __VA_ARGS__))                       \
  struct __FuzzyDefiner __VA_OPT__(: _FUZZY_APPLY_INHERITS(bases_tuple)) {                      \
    Class& __obj;                                                                               \
    ::fuzzy::ConstraintCollector& __cc;                                                         \
    _FUZZY_EXPAND(_FUZZY_FOR_EACH_CTX(_FUZZY_FIELD_MEMBER, Class, __VA_ARGS__))                 \
    explicit __FuzzyDefiner(Class& __obj_arg, ::fuzzy::ConstraintCollector& __cc_arg)             \
        : _FUZZY_BASE_CTORS_LEAD(bases_tuple) __obj(__obj_arg), __cc(__cc_arg)                  \
              _FUZZY_EXPAND(_FUZZY_FOR_EACH_CTX(_FUZZY_FIELD_INIT, Class, __VA_ARGS__)) {}       \
    void define() {                                                                             \
      _FUZZY_APPLY_BASE_DEFINES(bases_tuple)                                                     \
      _FUZZY_DSL_BIND(__cc)

#define BLOCK(name)        \
  __cc.begin_block(#name); \
  if (true)

#define FUZZY_END                                                                             \
  } /* define */                                                                               \
  }; /* __FuzzyDefiner */                                                                      \
  static void __fuzzy_define(__FuzzyClass& __obj, ::fuzzy::ConstraintCollector& __cc) {         \
    [[maybe_unused]] ::fuzzy::detail::ScopedBlockFilter __fuzzy_block_filter_guard{            \
        __cc,                                                                                  \
        +[](void* __fuzzy_p, const char* __fuzzy_n) -> bool {                                  \
          return static_cast<__FuzzyClass*>(__fuzzy_p)->__fuzzy_constraint_enabled(__fuzzy_n);  \
        },                                                                                     \
        &__obj};                                                                               \
    __FuzzyDefiner __d{__obj, __cc};                                                           \
    __d.define();                                                                              \
  }

/// Lvalue for the per-field runtime `rand_mode` flag (0 = state for next `randomize`, 1 = rand).
#define FUZZY_RAND_MODE(obj, field) ((obj).__fuzzy_rm_##field)

#define FUZZY_DECLARE_FUNC(fn) ::fuzzy::dsl::declare_func(fn)

#define FUZZY_CALL(obj, fn, ...) ::fuzzy::dsl::invoke((obj), (fn)__VA_OPT__(, ) __VA_ARGS__)

#endif  // FUZZY_FUZZY_H
