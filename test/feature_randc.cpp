#include <set>

#include "catch2/catch_all.hpp"
#include "fuzzy/Model.h"
#include "fuzzy/fuzzy.h"

TEST_CASE("randc: exhausts 0..3 before repeating on same object") {
  struct S {
    int y{};
    FUZZY_META(S, void, FUZZY_RANDC(y))
    BLOCK(c) {
      require(y >= 0);
      require(y <= 3);
    }
    FUZZY_END
  };

  fuzzy::Model<S> model{42U};
  S s{};
  std::set<int> seen;
  for (int i = 0; i < 4; ++i) {
    REQUIRE(model.randomize(s));
    REQUIRE(seen.insert(s.y).second);
  }
  CHECK(seen == std::set<int>{0, 1, 2, 3});
  REQUIRE(model.randomize(s));
  CHECK(seen.count(s.y) == 1);
}

TEST_CASE("randc: smaller feasible set 0..2 cycles three values") {
  struct S {
    int y{};
    FUZZY_META(S, void, FUZZY_RANDC(y))
    BLOCK(c) {
      require(y >= 0);
      require(y <= 2);
    }
    FUZZY_END
  };

  fuzzy::Model<S> model{7U};
  S s{};
  std::set<int> seen;
  for (int i = 0; i < 3; ++i) {
    REQUIRE(model.randomize(s));
    REQUIRE(seen.insert(s.y).second);
  }
  CHECK(seen == std::set<int>{0, 1, 2});
  REQUIRE(model.randomize(s));
  CHECK((0 <= s.y && s.y <= 2));
}
