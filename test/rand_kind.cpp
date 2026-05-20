#include "catch2/catch_all.hpp"
#include "fuzzy/fuzzy.h"
#include "fuzzy/Model.h"

TEST_CASE("state field stays fixed across randomize") {
  struct S {
    int x{};
    int z{5};
    FUZZY_META(S, void, FUZZY_RAND(x), z)
    BLOCK(c) {
      require(x > z);
    }
    FUZZY_END
  };

  fuzzy::Model<S> model;
  S s{};
  s.z = 7;
  REQUIRE(model.randomize(s));
  CHECK(s.x > 7);
  s.z = 20;
  REQUIRE(model.randomize(s));
  CHECK(s.x > 20);
}

TEST_CASE("FUZZY_RAND_MODE disables randomization for one field") {
  struct S {
    int x{};
    int y{};
    FUZZY_META(S, void, FUZZY_RAND(x), FUZZY_RAND(y))
    BLOCK(c) {
      require(x == 0);
      require(y == 1);
    }
    FUZZY_END
  };

  fuzzy::Model<S> model;
  S s{};
  s.x = 7;
  FUZZY_RAND_MODE(s, x) = false;
  CHECK_FALSE(fuzzy::validate(s));
  REQUIRE_FALSE(model.randomize(s));
  FUZZY_RAND_MODE(s, x) = true;
  s.x = 0;
  REQUIRE(model.randomize(s));
}

TEST_CASE("RANDC field still randomizes in a small domain (see feature_randc for cycle semantics)") {
  struct S {
    int y{};
    FUZZY_META(S, void, FUZZY_RANDC(y))
    BLOCK(c) {
      require(y >= 0);
      require(y <= 3);
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<S>();
  REQUIRE(r.has_value());
  CHECK(r->y >= 0);
  CHECK(r->y <= 3);
}

TEST_CASE("constraint_mode disables named BLOCK") {
  struct S {
    int x{};
    FUZZY_META(S, void, FUZZY_RAND(x))
    BLOCK(keep) {
      require(x == 0);
    }
    BLOCK(ignore) {
      require(x == 1);
    }
    FUZZY_END
  };

  fuzzy::Model<S> model;
  S s{};
  s.fuzzy_constraint_mode("ignore", 0);
  REQUIRE(model.randomize(s));
  CHECK(s.x == 0);
}

TEST_CASE("RandMember and RANDM use member rand_mode") {
  struct S {
    fuzzy::RandMember<int> a{};
    FUZZY_META(S, void, FUZZY_RANDM(a))
    BLOCK(c) {
      require(a >= 0);
      require(a <= 10);
    }
    FUZZY_END
  };

  fuzzy::Model<S> model;
  S s{};
  s.a.value = 3;
  s.a.rand_mode(0);
  REQUIRE(model.randomize(s));
  CHECK(s.a.value == 3);
}
