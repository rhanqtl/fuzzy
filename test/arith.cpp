#include "arith/model.h"

#include "catch2/catch_all.hpp"

TEST_CASE("Basic Arith") {
  Equation eqn;
  const auto ok = fuzzy::randomize(eqn);
  REQUIRE(ok);
  REQUIRE(fuzzy::validate(eqn));
}
