#include "catch2/catch_all.hpp"

#include "fuzzy/Model.h"
#include "fuzzy/fuzzy.h"

struct DiscardSoft {
  int x;
  int y;
  int z;

  FUZZY_META(DiscardSoft, void, FUZZY_RAND(x), FUZZY_RAND(y), FUZZY_RAND(z))
  BLOCK(c) {
    require(x + y == z);
    require(x > 0);
    require(y > 0);
    require(z > 0);
  }
  FUZZY_END
};

TEST_CASE("planner/single-step") {
  auto result = fuzzy::randomize<DiscardSoft>();
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
