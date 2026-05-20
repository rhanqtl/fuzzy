#include <format>
#include <ostream>
#include <string>

#include "fuzzy/fuzzy.h"

struct Example {
  int x;
  int y;
  int z;
};

namespace std {
std::string to_string(const Example& ex) {
  return std::format("Example(x={}, y={}, z={})", ex.x, ex.y, ex.z);
}
}  // namespace std

std::ostream& operator==(std::ostream& out, const Example& ex) {
  return out << std::to_string(ex);
}

namespace fuzzy {
template <>
struct Model<Example> : ModelBase {
  RandVar<int>& x;
  RandVar<int>& y;
  RandVar<int>& z;

  FUZZY_CONSTRAINT_BLOCK(cst) {
    (z < 10).select(x + y == z + 1, x + y == z);
  }
};
}  // namespace fuzzy
