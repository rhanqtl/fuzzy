#ifndef FUZZY_EXAMPLES_EQUATION_H
#define FUZZY_EXAMPLES_EQUATION_H

#include <format>
#include <iostream>

#include "fuzzy/fuzzy.h"

struct Equation {
  friend class fuzzy::Model<Equation>;

  int x;
  int y;
  int z;

 private:
  Equation() = default;
};

namespace fuzzy {
template <>
struct Model<Equation> {
  RandVar<int>& x;
  RandVar<int>& y;
  RandVar<int>& z;

  Model(Rand)
};
}  // namespace fuzzy

inline std::ostream& operator<<(std::ostream& outs, const Equation& eqn) {
  return outs << std::format("Equation{{.x = {}, .y = {}, .z = {}}}", eqn.x, eqn.y, eqn.z);
}
#endif  // FUZZY_EXAMPLES_EQUATION_H
