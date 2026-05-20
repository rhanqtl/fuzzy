#ifndef EXAMPLES_UNIQUE_PAIR_H
#define EXAMPLES_UNIQUE_PAIR_H

struct Pair {
  friend class fuzzy::Model<Pair>;

  FUZZY_RAND int x;
  FUZZY_RAND int y;

  FUZZY_CONSTRAINT(c) {
    unique(x, y);
  }
};

namespace fuzzy {
template <>
struct Model<Pair> {};
}  // namespace fuzzy

#endif  // EXAMPLES_UNIQUE_PAIR_H
