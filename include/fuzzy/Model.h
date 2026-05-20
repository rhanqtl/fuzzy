#ifndef FUZZY_MODEL_H
#define FUZZY_MODEL_H

#include <initializer_list>
#include <optional>
#include <random>
#include <string_view>
#include <utility>

#include "fuzzy/ConstraintCollector.h"
#include "fuzzy/dsl/Context.h"
#include "fuzzy/Evaluator.h"
#include "fuzzy/SolveStrategy.h"

namespace fuzzy {

namespace detail {

template <typename T>
void fuzzy_maybe_pre_randomize(T& obj) {
  if constexpr (requires(T& o) { o.fuzzy_pre_randomize(); }) {
    obj.fuzzy_pre_randomize();
  }
}

template <typename T>
void fuzzy_maybe_post_randomize(T& obj) {
  if constexpr (requires(T& o) { o.fuzzy_post_randomize(); }) {
    obj.fuzzy_post_randomize();
  }
}

/// One `__FuzzyDefiner` lifetime: class `define()` then inline `randomize with` body.
/// Avoids constructing `__FuzzyDefiner` twice (which would register duplicate proxies).
template <typename T, typename F>
void fuzzy_define_then_with(T& obj, ConstraintCollector& cc, F&& fn) {
  typename T::__FuzzyDefiner d{obj, cc};
  d.define();
  dsl::DslContext ctx{cc};
  std::forward<F>(fn)(d, ctx);
}

}  // namespace detail

// Forward declaration — SolveStrategy is defined separately
class SolveStrategy;

// Detect T has __fuzzy_define via FUZZY_META(Class, bases, ...)
template <typename T>
concept HasFuzzyMeta = requires(T& t, ConstraintCollector& cc) { T::__fuzzy_define(t, cc); };

// Generic Model — automatically available for all HasFuzzyMeta types
template <typename T>
class Model;

template <HasFuzzyMeta T>
class Model<T> {
 public:
  struct RandState {
    unsigned seed;
    unsigned call_count;
  };

  Model() :
      initial_seed_{std::random_device{}()} {}

  explicit Model(unsigned seed) :
      initial_seed_{seed} {}

  bool randomize(T& obj) {
    unsigned effective_seed = initial_seed_ + call_count_++;
    detail::fuzzy_maybe_pre_randomize(obj);
    ConstraintCollector cc;
    T::__fuzzy_define(obj, cc);
    cc.finalize();
    SolveStrategy strategy;
    if (!strategy.solve(cc, effective_seed)) {
      return false;
    }
    detail::fuzzy_maybe_post_randomize(obj);
    return true;
  }

  /// Randomize only the named fields for this call. Other rand/randc fields are
  /// pinned to their current values while still participating in constraints.
  bool randomize(T& obj, std::initializer_list<std::string_view> var_names) {
    unsigned effective_seed = initial_seed_ + call_count_++;
    detail::fuzzy_maybe_pre_randomize(obj);
    ConstraintCollector cc;
    cc.restrict_decision_vars_to(var_names);
    T::__fuzzy_define(obj, cc);
    cc.finalize();
    SolveStrategy strategy;
    if (!strategy.solve(cc, effective_seed)) {
      return false;
    }
    detail::fuzzy_maybe_post_randomize(obj);
    return true;
  }

  /// Single-call extra constraints (SystemVerilog `randomize() with { ... }` style).
  /// Runs class constraints from `define()`, then \p with_fn, then one `finalize` + solve.
  /// \p with_fn is invoked as `with_fn(definer, ctx)`; use `ctx.require(definer.<field> ...)`.
  template <typename WithFn>
  bool randomize(T& obj, WithFn&& with_fn) {
    unsigned effective_seed = initial_seed_ + call_count_++;
    detail::fuzzy_maybe_pre_randomize(obj);
    ConstraintCollector cc;
    detail::fuzzy_define_then_with<T>(obj, cc, std::forward<WithFn>(with_fn));
    cc.finalize();
    SolveStrategy strategy;
    if (!strategy.solve(cc, effective_seed)) {
      return false;
    }
    detail::fuzzy_maybe_post_randomize(obj);
    return true;
  }

  template <typename WithFn>
  bool randomize(T& obj, std::initializer_list<std::string_view> var_names, WithFn&& with_fn) {
    unsigned effective_seed = initial_seed_ + call_count_++;
    detail::fuzzy_maybe_pre_randomize(obj);
    ConstraintCollector cc;
    cc.restrict_decision_vars_to(var_names);
    detail::fuzzy_define_then_with<T>(obj, cc, std::forward<WithFn>(with_fn));
    cc.finalize();
    SolveStrategy strategy;
    if (!strategy.solve(cc, effective_seed)) {
      return false;
    }
    detail::fuzzy_maybe_post_randomize(obj);
    return true;
  }

  bool validate(const T& obj) {
    // const_cast is safe: we only read from obj through the proxies
    auto& mutable_obj = const_cast<T&>(obj);
    ConstraintCollector cc;
    T::__fuzzy_define(mutable_obj, cc);
    cc.finalize();
    Evaluator evaluator;
    return evaluator.validate(cc);
  }

  bool check(const T& obj) {
    return validate(obj);
  }

  /// Return the initial seed.
  unsigned seed() const {
    return initial_seed_;
  }

  /// Return how many times randomize() has been called.
  unsigned call_count() const {
    return call_count_;
  }

  /// Return the effective seed that will be used by the next randomize() call.
  unsigned next_seed() const {
    return initial_seed_ + call_count_;
  }

  /// Reset to a new initial seed and clear the call counter.
  void set_seed(unsigned seed) {
    initial_seed_ = seed;
    call_count_ = 0;
  }

  RandState rand_state() const {
    return RandState{initial_seed_, call_count_};
  }

  void set_rand_state(const RandState& state) {
    initial_seed_ = state.seed;
    call_count_ = state.call_count;
  }

 private:
  unsigned initial_seed_;
  unsigned call_count_{0};
};

// Convenience functions
template <HasFuzzyMeta T>
std::optional<T> randomize() {
  T obj{};
  Model<T> model;
  if (model.randomize(obj))
    return obj;
  return std::nullopt;
}

template <HasFuzzyMeta T>
std::optional<T> randomize(unsigned seed) {
  T obj{};
  Model<T> model(seed);
  if (model.randomize(obj))
    return obj;
  return std::nullopt;
}

template <HasFuzzyMeta T>
std::optional<T> randomize(std::initializer_list<std::string_view> var_names) {
  T obj{};
  Model<T> model;
  if (model.randomize(obj, var_names))
    return obj;
  return std::nullopt;
}

template <HasFuzzyMeta T>
std::optional<T> randomize(unsigned seed, std::initializer_list<std::string_view> var_names) {
  T obj{};
  Model<T> model(seed);
  if (model.randomize(obj, var_names))
    return obj;
  return std::nullopt;
}

/// `randomize() with { ... }` style factory (avoids overload clash with `randomize(unsigned)`).
template <HasFuzzyMeta T, typename WithFn>
std::optional<T> randomize_with(WithFn&& with_fn) {
  T obj{};
  Model<T> model;
  if (model.randomize(obj, std::forward<WithFn>(with_fn)))
    return obj;
  return std::nullopt;
}

template <HasFuzzyMeta T, typename WithFn>
std::optional<T> randomize_with(unsigned seed, WithFn&& with_fn) {
  T obj{};
  Model<T> model(seed);
  if (model.randomize(obj, std::forward<WithFn>(with_fn)))
    return obj;
  return std::nullopt;
}

template <HasFuzzyMeta T, typename WithFn>
std::optional<T> randomize_with(std::initializer_list<std::string_view> var_names, WithFn&& with_fn) {
  T obj{};
  Model<T> model;
  if (model.randomize(obj, var_names, std::forward<WithFn>(with_fn)))
    return obj;
  return std::nullopt;
}

template <HasFuzzyMeta T, typename WithFn>
std::optional<T> randomize_with(unsigned seed, std::initializer_list<std::string_view> var_names,
                                WithFn&& with_fn) {
  T obj{};
  Model<T> model(seed);
  if (model.randomize(obj, var_names, std::forward<WithFn>(with_fn)))
    return obj;
  return std::nullopt;
}

template <HasFuzzyMeta T, typename WithFn>
bool randomize_with(T& obj, WithFn&& with_fn) {
  Model<T> model;
  return model.randomize(obj, std::forward<WithFn>(with_fn));
}

template <HasFuzzyMeta T, typename WithFn>
bool randomize_with(T& obj, unsigned seed, WithFn&& with_fn) {
  Model<T> model(seed);
  return model.randomize(obj, std::forward<WithFn>(with_fn));
}

template <HasFuzzyMeta T>
bool randomize(T& obj, std::initializer_list<std::string_view> var_names) {
  Model<T> model;
  return model.randomize(obj, var_names);
}

template <HasFuzzyMeta T>
bool randomize(T& obj, unsigned seed, std::initializer_list<std::string_view> var_names) {
  Model<T> model(seed);
  return model.randomize(obj, var_names);
}

template <HasFuzzyMeta T, typename WithFn>
bool randomize_with(T& obj, std::initializer_list<std::string_view> var_names, WithFn&& with_fn) {
  Model<T> model;
  return model.randomize(obj, var_names, std::forward<WithFn>(with_fn));
}

template <HasFuzzyMeta T, typename WithFn>
bool randomize_with(T& obj, unsigned seed, std::initializer_list<std::string_view> var_names,
                    WithFn&& with_fn) {
  Model<T> model(seed);
  return model.randomize(obj, var_names, std::forward<WithFn>(with_fn));
}

template <HasFuzzyMeta T>
bool validate(const T& obj) {
  Model<T> model;
  return model.validate(obj);
}

template <HasFuzzyMeta T>
bool check(const T& obj) {
  Model<T> model;
  return model.check(obj);
}

}  // namespace fuzzy

#endif  // FUZZY_MODEL_H
