#ifndef FUZZY_CONSTRAINT_COLLECTOR_H
#define FUZZY_CONSTRAINT_COLLECTOR_H

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "fuzzy/ast/Expr.h"
#include "fuzzy/ast/RandArray.h"
#include "fuzzy/ast/RandMap.h"
#include "fuzzy/ast/RandVar.h"
#include "fuzzy/RandMember.h"

namespace fuzzy {

class ConstraintCollector;

namespace detail {

struct ScopedBlockFilter {
  using Fn = bool (*)(void*, const char*);
  ConstraintCollector& cc;
  ScopedBlockFilter(ConstraintCollector& c, Fn f, void* user);
  ~ScopedBlockFilter();
};

}  // namespace detail

class ConstraintCollector {
 public:
  using BlockFilterFn = bool (*)(void*, const char*);

  struct DistConstraint {
    Expr* expr;
    std::vector<std::pair<int64_t, int>> weighted_values;
  };

  struct Scope {
    std::vector<Expr*> constraints;
    std::vector<Expr*> soft_constraints;
    std::vector<Expr*> disabled_soft_constraints;
    std::vector<DistConstraint> dist_constraints;

    void clear() {
      constraints.clear();
      soft_constraints.clear();
      disabled_soft_constraints.clear();
      dist_constraints.clear();
    }
  };

  struct Block {
    std::string name;
    std::vector<Expr*> constraints;
    std::vector<Expr*> soft_constraints;
    std::vector<Expr*> disabled_soft_constraints;
    std::vector<DistConstraint> dist_constraints;
  };

  struct ForEachBody {
    std::vector<Expr*> constraints;
    std::vector<Expr*> soft_constraints;
  };

  ConstraintCollector() {
    // Initialize with a default scope
    scope_stack_.emplace_back();
    // Install arena so operators can allocate
    detail::set_arena(&arena_);
  }

  ~ConstraintCollector() {
    // Clear arena pointer
    if (detail::get_arena() == &arena_) {
      detail::set_arena(nullptr);
    }
  }

  ConstraintCollector(const ConstraintCollector&) = delete;
  ConstraintCollector& operator=(const ConstraintCollector&) = delete;

  void set_block_filter(BlockFilterFn f, void* user) {
    block_filter_ = f;
    block_user_ = user;
  }

  void clear_block_filter() {
    block_filter_ = nullptr;
    block_user_ = nullptr;
  }

  void clear_decision_var_restriction() {
    restrict_decision_vars_ = false;
    restricted_decision_var_names_.clear();
  }

  void restrict_decision_vars_to(std::initializer_list<std::string_view> names) {
    restricted_decision_var_names_.clear();
    restricted_decision_var_names_.reserve(names.size());
    for (std::string_view name : names) {
      restricted_decision_var_names_.emplace(name);
    }
    restrict_decision_vars_ = true;
  }

  bool is_call_decision_var(const VarExprBase& var) const {
    if (!restrict_decision_vars_) {
      return var.is_decision_var_for_solve();
    }
    if (var.is_symbolic()) {
      return true;
    }
    if (var.compile_kind() == CompileRandKind::State) {
      return false;
    }
    if (!var.rand_mode_enabled()) {
      return false;
    }
    return restricted_decision_var_names_.contains(var.name());
  }

  bool decision_vars_are_restricted() const {
    return restrict_decision_vars_;
  }

  // ---- Proxy creation (overload dispatch) ----

  template <std::integral T>
  VarExpr<T>& create_proxy(T& out, const std::string& name, CompileRandKind compile_kind,
                           bool* rand_mode_ptr, RandcCycle* randc_cycle = nullptr) {
    auto ptr = std::make_unique<VarExpr<T>>(out, name.empty() ? auto_name() : name, compile_kind,
                                            rand_mode_ptr, randc_cycle);
    VarExpr<T>& ref = *ptr;
    all_vars_.push_back(ptr.get());
    arena_.nodes.push_back(std::move(ptr));
    return ref;
  }

  template <typename E>
    requires std::is_enum_v<std::remove_cvref_t<E>>
  VarExprEnum<std::remove_cvref_t<E>>& create_proxy(E& out, const std::string& name,
                                                     CompileRandKind compile_kind, bool* rand_mode_ptr,
                                                     RandcCycle* randc_cycle = nullptr) {
    using U = std::remove_cvref_t<E>;
    auto ptr = std::make_unique<VarExprEnum<U>>(out, name.empty() ? auto_name() : name, compile_kind,
                                                 rand_mode_ptr, randc_cycle);
    VarExprEnum<U>& ref = *ptr;
    all_vars_.push_back(ptr.get());
    arena_.nodes.push_back(std::move(ptr));
    return ref;
  }

  template <std::integral T>
  VarExpr<T>& create_proxy(RandMember<T>& out, const std::string& name, CompileRandKind compile_kind) {
    return create_proxy(out.ref(), name, compile_kind, &out.fuzzy_rand_mode_, nullptr);
  }

  // Fallback for user-defined scalar/object fields that are not directly
  // representable as symbolic arithmetic expressions in the current DSL.
  template <typename T>
    requires(!std::integral<T> && !std::same_as<std::remove_cvref_t<T>, std::string> &&
             !std::is_enum_v<std::remove_cvref_t<T>>)
  T& create_proxy(T& out, const std::string& = "") {
    return out;
  }

  template <typename T>
    requires(!std::integral<T> && !std::same_as<std::remove_cvref_t<T>, std::string> &&
             !std::is_enum_v<std::remove_cvref_t<T>>)
  T& create_proxy(T& out, const std::string&, CompileRandKind, bool*) {
    return out;
  }

  template <std::integral T>
  ArrayExpr<T>& create_proxy(std::vector<T>& out, const std::string& name = "") {
    auto ptr = std::make_unique<ArrayExpr<T>>(out, name.empty() ? auto_name() : name);
    ArrayExpr<T>& ref = *ptr;
    all_arrays_.push_back(ptr.get());
    // Also register the size variable for phase-1 solving
    all_vars_.push_back(&ref.size_var());
    arena_.nodes.push_back(std::move(ptr));
    return ref;
  }

  template <std::integral T>
  ArrayExpr<T>& create_proxy(std::vector<T>& out, const std::string& name, CompileRandKind compile_kind,
                             bool* rand_mode_ptr) {
    (void)compile_kind;
    (void)rand_mode_ptr;
    return create_proxy(out, name);
  }

  template <typename T>
    requires(!std::integral<T>)
  std::vector<T>& create_proxy(std::vector<T>& out, const std::string& name, CompileRandKind compile_kind,
                               bool* rand_mode_ptr) {
    (void)compile_kind;
    (void)rand_mode_ptr;
    return create_proxy(out, name);
  }

  // Fallback for vectors of user-defined element types.
  template <typename T>
    requires(!std::integral<T>)
  std::vector<T>& create_proxy(std::vector<T>& out, const std::string& = "") {
    return out;
  }

  template <typename K, typename V, typename Hash, typename Eq>
    requires(RandMapKeyType<K> && RandMapValueType<V>)
  RandMap<K, V, Hash, Eq>& create_proxy(std::unordered_map<K, V, Hash, Eq>& out,
                                        const std::string& name = "") {
    auto ptr = std::make_unique<RandMap<K, V, Hash, Eq>>(out, name.empty() ? auto_name() : name);
    RandMap<K, V, Hash, Eq>& ref = *ptr;
    all_maps_.push_back(ptr.get());
    all_vars_.push_back(&ref.size());
    arena_.nodes.push_back(std::move(ptr));
    return ref;
  }

  template <typename K, typename V, typename Hash, typename Eq>
    requires(RandMapKeyType<K> && RandMapValueType<V>)
  RandMap<K, V, Hash, Eq>& create_proxy(std::unordered_map<K, V, Hash, Eq>& out,
                                        const std::string& name, CompileRandKind compile_kind,
                                        bool* rand_mode_ptr) {
    (void)compile_kind;
    (void)rand_mode_ptr;
    return create_proxy(out, name);
  }

  template <typename K, typename V, typename Hash, typename Eq>
    requires(!(RandMapKeyType<K> && RandMapValueType<V>))
  std::unordered_map<K, V, Hash, Eq>& create_proxy(std::unordered_map<K, V, Hash, Eq>& out,
                                                   const std::string& = "") {
    return out;
  }

  template <typename K, typename V, typename Compare, typename Alloc>
    requires(RandMapKeyType<K> && RandMapValueType<V>)
  RandOrderedMap<K, V, Compare, Alloc>& create_proxy(std::map<K, V, Compare, Alloc>& out,
                                                     const std::string& name = "") {
    auto ptr =
        std::make_unique<RandOrderedMap<K, V, Compare, Alloc>>(out, name.empty() ? auto_name() : name);
    RandOrderedMap<K, V, Compare, Alloc>& ref = *ptr;
    all_maps_.push_back(ptr.get());
    all_vars_.push_back(&ref.size());
    arena_.nodes.push_back(std::move(ptr));
    return ref;
  }

  template <typename K, typename V, typename Compare, typename Alloc>
    requires(RandMapKeyType<K> && RandMapValueType<V>)
  RandOrderedMap<K, V, Compare, Alloc>& create_proxy(std::map<K, V, Compare, Alloc>& out,
                                                      const std::string& name,
                                                      CompileRandKind compile_kind, bool* rand_mode_ptr) {
    (void)compile_kind;
    (void)rand_mode_ptr;
    return create_proxy(out, name);
  }

  template <typename K, typename V, typename Compare, typename Alloc>
    requires(!(RandMapKeyType<K> && RandMapValueType<V>))
  std::map<K, V, Compare, Alloc>& create_proxy(std::map<K, V, Compare, Alloc>& out,
                                               const std::string& = "") {
    return out;
  }

  // Create unbound symbolic variable (for for_each_i)
  template <std::integral T>
  VarExpr<T>& create_symbolic(const std::string& name) {
    auto ptr = std::make_unique<VarExpr<T>>(name);
    VarExpr<T>& ref = *ptr;
    arena_.nodes.push_back(std::move(ptr));
    return ref;
  }

  // ---- Constraint collection ----

  void add_constraint(Expr& e) {
    assert(!scope_stack_.empty());
    if (!current_block_accepts_) {
      return;
    }
    scope_stack_.back().constraints.push_back(&e);
  }

  void add_soft_constraint(Expr& e) {
    assert(!scope_stack_.empty());
    if (!current_block_accepts_) {
      return;
    }
    scope_stack_.back().soft_constraints.push_back(&e);
  }

  void disable_soft_constraint(Expr& e) {
    assert(!scope_stack_.empty());
    if (!current_block_accepts_) {
      return;
    }
    scope_stack_.back().disabled_soft_constraints.push_back(&e);
  }

  // Scope stack (for for_each_i nesting)
  void push_scope() {
    scope_stack_.push_back(Scope{});
  }

  Scope pop_scope() {
    assert(scope_stack_.size() > 1 && "Cannot pop the root scope");
    auto top = std::move(scope_stack_.back());
    scope_stack_.pop_back();
    return top;
  }

  // ---- Block management ----

  void begin_block(const char* name) {
    current_block_accepts_ = !block_filter_ || block_filter_(block_user_, name);
    auto& cur = scope_stack_.back();
    if (!cur.constraints.empty() || !cur.soft_constraints.empty() ||
        !cur.disabled_soft_constraints.empty() || !cur.dist_constraints.empty()) {
      if (blocks_.empty()) {
        blocks_.push_back(Block{"default", {}, {}, {}, {}});
      }
      for (auto* c : cur.constraints) {
        blocks_.back().constraints.push_back(c);
      }
      for (auto* c : cur.soft_constraints) {
        blocks_.back().soft_constraints.push_back(c);
      }
      for (auto* c : cur.disabled_soft_constraints) {
        blocks_.back().disabled_soft_constraints.push_back(c);
      }
      for (auto& d : cur.dist_constraints) {
        blocks_.back().dist_constraints.push_back(std::move(d));
      }
      cur.clear();
    }
    blocks_.push_back(Block{name, {}, {}, {}, {}});
  }

  // Flush remaining constraints into the last block
  void finalize() {
    if (blocks_.empty()) {
      blocks_.push_back(Block{"default", {}, {}, {}, {}});
    }
    auto& cur = scope_stack_.back();
    for (auto* c : cur.constraints) {
      blocks_.back().constraints.push_back(c);
    }
    for (auto* c : cur.soft_constraints) {
      blocks_.back().soft_constraints.push_back(c);
    }
    for (auto* c : cur.disabled_soft_constraints) {
      blocks_.back().disabled_soft_constraints.push_back(c);
    }
    for (auto& d : cur.dist_constraints) {
      blocks_.back().dist_constraints.push_back(std::move(d));
    }
    cur.clear();
  }

  // ---- Higher-order constraints ----

  template <typename F>
  void for_each_i(ArrayExprBase& arr, F&& body) {
    // 1. Create symbolic variables
    auto& sym_idx = create_symbolic<std::size_t>("__idx_" + std::to_string(foreach_counter_++));
    auto& sym_elem = arr.symbolic_element();

    // 2. Push sub-scope, execute lambda symbolically
    push_scope();
    body(sym_idx, sym_elem);
    auto body_scope = pop_scope();

    add_for_each_i_expr(arr, sym_idx, sym_elem, std::move(body_scope.constraints), false);
    add_for_each_i_expr(arr, sym_idx, sym_elem, std::move(body_scope.soft_constraints), true);
  }

  // Fallback for arrays/vectors that are not representable as symbolic arrays.
  // We intentionally no-op here so user code remains compilable.
  template <typename Arr, typename F>
    requires(!std::derived_from<std::remove_reference_t<Arr>, ArrayExprBase>)
  void for_each_i(Arr&, F&&) {}

  template <typename MapT, typename F>
  void for_each_kv(BasicRandMap<MapT>& map, F&& body) {
    auto& sym_idx = create_symbolic<std::size_t>("__kv_idx_" + std::to_string(foreach_kv_counter_++));
    auto& sym_key = map.symbolic_key_typed();
    auto& sym_val = map.symbolic_value_typed();

    push_scope();
    body(sym_idx, sym_key, sym_val);
    auto body_scope = pop_scope();

    add_for_each_kv_expr(map, sym_idx, sym_key, sym_val, std::move(body_scope.constraints), false);
    add_for_each_kv_expr(map, sym_idx, sym_key, sym_val, std::move(body_scope.soft_constraints), true);
  }

  template <typename M, typename F>
    requires(!std::derived_from<std::remove_reference_t<M>, MapExprBase>)
  void for_each_kv(M&, F&&) {}

  void add_unique(ArrayExprBase& arr) {
    auto ptr = std::make_unique<UniqueExpr>(&arr);
    UniqueExpr& ref = *ptr;
    arena_.nodes.push_back(std::move(ptr));
    add_constraint(ref);
  }

  template <typename First, typename... Rest>
  void add_unique(First& first, Rest&... rest) {
    std::vector<Expr*> exprs = {to_expr_ptr(first), to_expr_ptr(rest)...};
    for (std::size_t i = 0; i < exprs.size(); i++) {
      for (std::size_t j = i + 1; j < exprs.size(); j++) {
        auto& ne = detail::make_binary(ExprKind::NotEqual, *exprs[i], *exprs[j]);
        add_constraint(ne);
      }
    }
  }

  template <typename ExprLike>
  void add_dist(ExprLike& expr_like,
                std::initializer_list<std::pair<int64_t, int>> weighted_values) {
    assert(!scope_stack_.empty());
    if (!current_block_accepts_) {
      return;
    }
    Expr* expr = to_expr_ptr(expr_like);
    Expr* domain = nullptr;
    std::vector<std::pair<int64_t, int>> filtered_values;
    for (const auto& [value, weight] : weighted_values) {
      if (weight <= 0) {
        continue;
      }
      auto& eq = detail::make_binary(ExprKind::Equal, *expr, detail::make_const(value));
      if (!domain) {
        domain = &eq;
      } else {
        domain = &detail::make_binary(ExprKind::LogicalOr, *domain, eq);
      }
      filtered_values.emplace_back(value, weight);
    }
    if (domain) {
      add_constraint(*domain);
      scope_stack_.back().dist_constraints.push_back(DistConstraint{expr, std::move(filtered_values)});
    }
  }

  void add_solve_before(Expr& lhs, Expr& rhs) {
    solve_before_.emplace_back(&lhs, &rhs);
  }

  Expr& make_implies(Expr& cond, Expr& body) {
    auto ptr = std::make_unique<ImpliesExpr>(&cond, &body);
    ImpliesExpr& ref = *ptr;
    arena_.nodes.push_back(std::move(ptr));
    return ref;
  }

  Expr& make_array_agg(ArrayAggKind agg, ArrayExprBase& array) {
    auto ptr = std::make_unique<ArrayAggExpr>(agg, &array);
    ArrayAggExpr& ref = *ptr;
    arena_.nodes.push_back(std::move(ptr));
    return ref;
  }

  template <typename F>
  Expr& make_array_agg(ArrayAggKind agg, ArrayExprBase& array, F&& with_fn) {
    auto& sym_idx = create_symbolic<std::size_t>("__agg_idx_" + std::to_string(array_agg_counter_++));
    auto& sym_elem = array.symbolic_element();
    Expr& value = std::forward<F>(with_fn)(sym_idx, sym_elem);
    auto ptr = std::make_unique<ArrayAggExpr>(agg, &array, &sym_idx, &sym_elem, &value);
    ArrayAggExpr& ref = *ptr;
    arena_.nodes.push_back(std::move(ptr));
    return ref;
  }

  Expr& make_false() {
    return detail::make_binary(ExprKind::Equal, detail::make_const(0), detail::make_const(1));
  }

  /// SV `if (cond) { ... }` constraint block: each inner `require` becomes `cond -> expr`.
  template <typename F>
  void constraint_if(Expr& cond, F&& then_fn) {
    push_scope();
    std::forward<F>(then_fn)();
    auto body = pop_scope();
    for (auto* c : body.constraints) {
      add_constraint(make_implies(cond, *c));
    }
    for (auto* s : body.soft_constraints) {
      add_soft_constraint(make_implies(cond, *s));
    }
    for (auto* s : body.disabled_soft_constraints) {
      disable_soft_constraint(make_implies(cond, *s));
    }
  }

  /// SV `if (cond) { ... } else { ... }` constraint block.
  template <typename F, typename G>
  void constraint_if_else(Expr& cond, F&& then_fn, G&& else_fn) {
    push_scope();
    std::forward<F>(then_fn)();
    auto th = pop_scope();
    push_scope();
    std::forward<G>(else_fn)();
    auto el = pop_scope();
    Expr& ncond = detail::make_unary(ExprKind::LogicalNot, cond);
    for (auto* c : th.constraints) {
      add_constraint(make_implies(cond, *c));
    }
    for (auto* s : th.soft_constraints) {
      add_soft_constraint(make_implies(cond, *s));
    }
    for (auto* s : th.disabled_soft_constraints) {
      disable_soft_constraint(make_implies(cond, *s));
    }
    for (auto* c : el.constraints) {
      add_constraint(make_implies(ncond, *c));
    }
    for (auto* s : el.soft_constraints) {
      add_soft_constraint(make_implies(ncond, *s));
    }
    for (auto* s : el.disabled_soft_constraints) {
      disable_soft_constraint(make_implies(ncond, *s));
    }
  }

  // ---- Result access ----

  const std::vector<Block>& blocks() const {
    return blocks_;
  }
  std::vector<Block>& blocks() {
    return blocks_;
  }

  // Access all registered variable/array proxies
  const std::vector<VarExprBase*>& all_vars() const {
    return all_vars_;
  }
  const std::vector<ArrayExprBase*>& all_arrays() const {
    return all_arrays_;
  }
  const std::vector<MapExprBase*>& all_maps() const {
    return all_maps_;
  }

  const std::vector<std::pair<Expr*, Expr*>>& solve_before() const {
    return solve_before_;
  }

  // ---- Arena access ----

  detail::Arena& arena() {
    return arena_;
  }

 private:
  template <typename T>
  static Expr* to_expr_ptr(T& value) {
    if constexpr (std::derived_from<std::remove_reference_t<T>, Expr>) {
      return &value;
    } else {
      static_assert(std::derived_from<std::remove_reference_t<T>, Expr>,
                    "unique/dist arguments must be Expr-like");
      return nullptr;
    }
  }

  std::string auto_name() {
    return "__v" + std::to_string(name_counter_++);
  }

  void add_for_each_i_expr(ArrayExprBase& arr, Expr& sym_idx, Expr& sym_elem, std::vector<Expr*> body,
                           bool is_soft) {
    if (body.empty()) {
      return;
    }
    auto ptr = std::make_unique<ForEachIExpr>(&arr, &sym_idx, &sym_elem, std::move(body));
    ForEachIExpr& ref = *ptr;
    arena_.nodes.push_back(std::move(ptr));
    if (is_soft) {
      add_soft_constraint(ref);
    } else {
      add_constraint(ref);
    }
  }

  void add_for_each_kv_expr(MapExprBase& map, Expr& sym_idx, Expr& sym_key, Expr& sym_val,
                            std::vector<Expr*> body, bool is_soft) {
    if (body.empty()) {
      return;
    }
    auto ptr = std::make_unique<ForEachKVExpr>(&map, &sym_idx, &sym_key, &sym_val, std::move(body));
    ForEachKVExpr& ref = *ptr;
    arena_.nodes.push_back(std::move(ptr));
    if (is_soft) {
      add_soft_constraint(ref);
    } else {
      add_constraint(ref);
    }
  }

  std::vector<Block> blocks_;
  std::vector<Scope> scope_stack_;
  detail::Arena arena_;

  std::vector<VarExprBase*> all_vars_;
  std::vector<ArrayExprBase*> all_arrays_;
  std::vector<MapExprBase*> all_maps_;
  std::vector<std::pair<Expr*, Expr*>> solve_before_;

  int name_counter_{0};
  int foreach_counter_{0};
  int foreach_kv_counter_{0};
  int array_agg_counter_{0};

  BlockFilterFn block_filter_{nullptr};
  void* block_user_{nullptr};
  bool current_block_accepts_{true};
  bool restrict_decision_vars_{false};
  std::unordered_set<std::string> restricted_decision_var_names_;
};

inline detail::ScopedBlockFilter::ScopedBlockFilter(ConstraintCollector& c, Fn f, void* user) :
    cc(c) {
  cc.set_block_filter(f, user);
}

inline detail::ScopedBlockFilter::~ScopedBlockFilter() {
  cc.clear_block_filter();
}

}  // namespace fuzzy

#endif  // FUZZY_CONSTRAINT_COLLECTOR_H
