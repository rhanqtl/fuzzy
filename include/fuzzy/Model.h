#ifndef FUZZY_MODEL_H
#define FUZZY_MODEL_H

#include <optional>
#include <random>

#include "fuzzy/ConstraintCollector.h"
#include "fuzzy/Evaluator.h"
#include "fuzzy/SolveStrategy.h"

namespace fuzzy {

// Forward declaration — SolveStrategy is defined separately
class SolveStrategy;

// Detect T has __fuzzy_define via FUZZY_META
template <typename T>
concept HasFuzzyMeta = requires(T& t, ConstraintCollector& cc) { T::__fuzzy_define(t, cc); };

// Generic Model — automatically available for all HasFuzzyMeta types
template <typename T>
class Model;

template <HasFuzzyMeta T>
class Model<T> {
 public:
  Model() :
      initial_seed_{std::random_device{}()} {}

  explicit Model(unsigned seed) :
      initial_seed_{seed} {}

  bool randomize(T& obj) {
    unsigned effective_seed = initial_seed_ + call_count_++;
    ConstraintCollector cc;
    T::__fuzzy_define(obj, cc);
    SolveStrategy strategy;
    return strategy.solve(cc, effective_seed);
  }

  bool validate(const T& obj) {
    // const_cast is safe: we only read from obj through the proxies
    auto& mutable_obj = const_cast<T&>(obj);
    ConstraintCollector cc;
    T::__fuzzy_define(mutable_obj, cc);
    Evaluator evaluator;
    return evaluator.validate(cc);
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
bool validate(const T& obj) {
  Model<T> model;
  return model.validate(obj);
}

}  // namespace fuzzy

#endif  // FUZZY_MODEL_H
