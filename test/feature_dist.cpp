#include "catch2/catch_all.hpp"
#include "fuzzy/fuzzy.h"

TEST_CASE("dist skips non-positive weights") {
  struct A {
    int x{0};
    FUZZY_META(A, void, FUZZY_RAND(x))
    BLOCK(c) {
      dist(x, {{0, 0}, {1, 1}, {2, 0}, {3, 2}});
      require(x >= 0);
      require(x <= 10);
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<A>();
  REQUIRE(r.has_value());
  CHECK(((r->x == 1) || (r->x == 3)));
  CHECK(fuzzy::validate(*r));
}

TEST_CASE("dist weights drive feasible candidate choice") {
  struct A {
    int x{0};
    FUZZY_META(A, void, FUZZY_RAND(x))
    BLOCK(c) {
      dist(x, {{0, 2}, {5, 1}});
      require(x == 0 || x == 5);
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<A>(99u);
  REQUIRE(r.has_value());
  CHECK(fuzzy::validate(*r));
}

TEST_CASE("dist varies choices across seeds") {
  struct A {
    int x{0};
    FUZZY_META(A, void, FUZZY_RAND(x))
    BLOCK(c) {
      dist(x, {{0, 1}, {1, 1}});
    }
    FUZZY_END
  };

  bool saw_zero = false;
  bool saw_one = false;
  for (unsigned seed = 1; seed <= 32; ++seed) {
    auto r = fuzzy::randomize<A>(seed);
    REQUIRE(r.has_value());
    CHECK(((r->x == 0) || (r->x == 1)));
    saw_zero = saw_zero || r->x == 0;
    saw_one = saw_one || r->x == 1;
  }

  CHECK(saw_zero);
  CHECK(saw_one);
}

TEST_CASE("dist skips infeasible weighted candidates") {
  struct A {
    int x{0};
    FUZZY_META(A, void, FUZZY_RAND(x))
    BLOCK(c) {
      dist(x, {{0, 100}, {7, 1}});
      require(x == 7);
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<A>(5u);
  REQUIRE(r.has_value());
  CHECK(r->x == 7);
  CHECK(fuzzy::validate(*r));
}

TEST_CASE("dist can target scalar expressions") {
  struct A {
    int x{0};
    int y{0};
    FUZZY_META(A, void, FUZZY_RAND(x), FUZZY_RAND(y))
    BLOCK(c) {
      require(x >= 0);
      require(x <= 10);
      require(y >= 0);
      require(y <= 10);
      dist(x + y, {{3, 1}, {8, 1}});
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<A>(17u);
  REQUIRE(r.has_value());
  CHECK(((r->x + r->y == 3) || (r->x + r->y == 8)));
  CHECK(fuzzy::validate(*r));
}
