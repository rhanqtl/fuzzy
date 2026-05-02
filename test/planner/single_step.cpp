#include "catch2/catch_all.hpp"

#include "fuzzy/Model.h"
#include "fuzzy/fuzzy.h"

struct Equation {
  int x;
  int y;
  int z;

  FUZZY_META(Equation, x, y, z)
    BLOCK(c) {
      constrain(x + y == z);
      constrain(x > 0);
      constrain(y > 0);
      constrain(z > 0);
    }
  FUZZY_END
};

TEST_CASE("planner/single-step") {
  auto result = fuzzy::randomize<Equation>();
  REQUIRE(result.has_value());
  const auto x = result->x;
  const auto y = result->y;
  const auto z = result->z;
  CHECK(x + y == z);
  CHECK(x > 0);
  CHECK(y > 0);
  CHECK(z > 0);
  CHECK(fuzzy::validate(*result));
}
