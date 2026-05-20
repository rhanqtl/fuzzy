#include "catch2/catch_all.hpp"
#include "fuzzy/fuzzy.h"

enum class Axis : int { X = 0, Y = 1, Z = 2 };

TEST_CASE("FUZZY_RAND on enum class field solves and write-back") {
  struct S {
    Axis a{Axis::X};
    FUZZY_META(S, void, FUZZY_RAND(a))
    BLOCK(c) {
      require(a >= static_cast<int>(0));
      require(a <= static_cast<int>(2));
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<S>();
  REQUIRE(r.has_value());
  const int v = static_cast<int>(r->a);
  CHECK(v >= 0);
  CHECK(v <= 2);
  CHECK(fuzzy::validate(*r));
}

TEST_CASE("FUZZY_RANDC on enum class permutes three values") {
  struct S {
    Axis a{Axis::X};
    FUZZY_META(S, void, FUZZY_RANDC(a))
    BLOCK(c) {
      require(a >= static_cast<int>(0));
      require(a <= static_cast<int>(2));
    }
    FUZZY_END
  };

  fuzzy::Model<S> model{99U};
  S s{};
  int v0 = -1;
  int v1 = -1;
  int v2 = -1;
  REQUIRE(model.randomize(s));
  v0 = static_cast<int>(s.a);
  REQUIRE(model.randomize(s));
  v1 = static_cast<int>(s.a);
  REQUIRE(model.randomize(s));
  v2 = static_cast<int>(s.a);
  CHECK(v0 != v1);
  CHECK(v0 != v2);
  CHECK(v1 != v2);
  REQUIRE(model.randomize(s));
  const int v3 = static_cast<int>(s.a);
  CHECK(v3 >= 0);
  CHECK(v3 <= 2);
}
