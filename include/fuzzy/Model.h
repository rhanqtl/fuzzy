#ifndef FUZZY_MODEL_H
#define FUZZY_MODEL_H

#include <optional>

#include "fuzzy/ConstraintCollector.h"
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
  bool randomize(T& obj) {
    ConstraintCollector cc;
    T::__fuzzy_define(obj, cc);
    SolveStrategy strategy;
    return strategy.solve(cc);
  }
};

// Convenience function
template <HasFuzzyMeta T>
std::optional<T> randomize() {
  T obj{};
  Model<T> model;
  if (model.randomize(obj))
    return obj;
  return std::nullopt;
}

}  // namespace fuzzy

#endif  // FUZZY_MODEL_H
