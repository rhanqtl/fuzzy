#include "catch2/catch_all.hpp"
#include "fuzzy/fuzzy.h"

TEST_CASE("solve_before biases order of assignment") {
  struct A {
    int a{0};
    int b{0};
    FUZZY_META(A, void, FUZZY_RAND(a), FUZZY_RAND(b))
    BLOCK(c) {
      require(a >= 0);
      require(a <= 100);
      require(b >= 0);
      require(b <= 100);
      solve_before(a, b);
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<A>(42u);
  REQUIRE(r.has_value());
  CHECK(fuzzy::validate(*r));
}

TEST_CASE("solve_before rejects randc ordering variables") {
  struct A {
    int a{0};
    int b{0};
    FUZZY_META(A, void, FUZZY_RANDC(a), FUZZY_RAND(b))
    BLOCK(c) {
      require(a >= 0);
      require(a <= 3);
      require(b >= 0);
      require(b <= 3);
      solve_before(a, b);
    }
    FUZZY_END
  };

  CHECK_FALSE(fuzzy::randomize<A>().has_value());
}
