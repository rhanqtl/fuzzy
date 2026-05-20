#include "catch2/catch_all.hpp"

#include "fuzzy/fuzzy.h"

struct UniqueScalarTriple {
  int x;
  int y;
  int z;

  FUZZY_META(UniqueScalarTriple, void, FUZZY_RAND(x), FUZZY_RAND(y), FUZZY_RAND(z))
  BLOCK(c) {
    require(0 <= x && x <= 2);
    require(0 <= y && y <= 2);
    require(0 <= z && z <= 2);
    unique(x, y, z);
  }
  FUZZY_END
};

struct SoftConflict {
  int x;

  FUZZY_META(SoftConflict, void, FUZZY_RAND(x))
  BLOCK(c) {
    require(0 <= x && x <= 1);
    soft(x == 0);
    soft(x == 1);
    soft(x == 2);
  }
  FUZZY_END
};

struct DistChoice {
  int x;

  FUZZY_META(DistChoice, void, FUZZY_RAND(x))
  BLOCK(c) {
    require(0 <= x && x <= 3);
    dist(x, {{1, 1}, {3, 2}});
  }
  FUZZY_END
};

struct SolveBeforeHint {
  int a;
  int b;

  FUZZY_META(SolveBeforeHint, void, FUZZY_RAND(a), FUZZY_RAND(b))
  BLOCK(c) {
    require(0 <= a && a <= 1);
    require(0 <= b && b <= 1);
    solve_before(a, b);
    soft(a == 0);
    soft(b == 1);
  }
  FUZZY_END
};

TEST_CASE("planner/advanced-constraints/unique-scalars") {
  auto result = fuzzy::randomize<UniqueScalarTriple>(7);
  REQUIRE(result.has_value());
  CHECK(result->x != result->y);
  CHECK(result->x != result->z);
  CHECK(result->y != result->z);
  CHECK(fuzzy::validate(*result));
}

TEST_CASE("planner/advanced-constraints/soft") {
  auto result = fuzzy::randomize<SoftConflict>(11);
  REQUIRE(result.has_value());
  CHECK(result->x >= 0);
  CHECK(result->x <= 1);
  CHECK(fuzzy::validate(*result));
}

TEST_CASE("planner/advanced-constraints/dist") {
  auto result = fuzzy::randomize<DistChoice>(23);
  REQUIRE(result.has_value());
  CHECK((result->x == 1 || result->x == 3));
  CHECK(fuzzy::validate(*result));
}

TEST_CASE("planner/advanced-constraints/solve-before") {
  auto result = fuzzy::randomize<SolveBeforeHint>(31);
  REQUIRE(result.has_value());
  CHECK(result->a >= 0);
  CHECK(result->a <= 1);
  CHECK(result->b >= 0);
  CHECK(result->b <= 1);
  CHECK(fuzzy::validate(*result));
}
