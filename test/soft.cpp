#include "catch2/catch_all.hpp"

#include "fuzzy/fuzzy.h"

TEST_CASE("soft constraints should be discarded when unsat") {
  struct A {
    int x{};
    int y{};
  
    FUZZY_META(A, void, FUZZY_RAND(x), FUZZY_RAND(y))
      BLOCK(c) {
        require(x + y == 0);
        require(x > 0);
        soft(y > 0);
      }
    FUZZY_END
  };

  auto result = fuzzy::randomize<A>();
  REQUIRE(result.has_value());
  const auto x = result->x;
  const auto y = result->y;
  CHECK(x + y == 0);
  CHECK(x > 0);
  CHECK(y < 0);
  CHECK(fuzzy::validate(*result));
}

TEST_CASE("soft constraints should be discarded by reverse text order") {
  struct A {
    int x{};
    int y{};
  
    FUZZY_META(A, void, FUZZY_RAND(x), FUZZY_RAND(y))
      BLOCK(c) {
        require(x + y == 0);
        soft(x > 0);
        soft(y > 0);
      }
    FUZZY_END
  };

  auto result = fuzzy::randomize<A>();
  REQUIRE(result.has_value());
  const auto x = result->x;
  const auto y = result->y;
  CHECK(x + y == 0);
  CHECK(x > 0);
  CHECK(y < 0);
  CHECK(fuzzy::validate(*result));
}

TEST_CASE("soft constraints inside for_each stay soft") {
  struct A {
    std::vector<int> xs;

    FUZZY_META(A, void, FUZZY_RAND(xs))
    BLOCK(c) {
      require(xs.size() == 2);
      for_each_i(xs, [&](auto&, auto& e) {
        require(e >= 0);
        soft(e == 1);
      });
      require(array_sum(xs) == 0);
    }
    FUZZY_END
  };

  auto result = fuzzy::randomize<A>();
  REQUIRE(result.has_value());
  CHECK(result->xs.size() == 2);
  CHECK(result->xs[0] == 0);
  CHECK(result->xs[1] == 0);
  CHECK(fuzzy::validate(*result));
}

TEST_CASE("disable_soft removes matching soft constraint for one call") {
  struct A {
    int x{0};

    FUZZY_META(A, void, FUZZY_RAND(x))
    BLOCK(c) {
      require(x >= 0);
      require(x <= 10);
      soft(x == 1);
      soft(x == 2);
    }
    FUZZY_END
  };

  fuzzy::Model<A> model(7u);
  A baseline{};
  REQUIRE(model.randomize(baseline));
  CHECK(baseline.x == 1);

  A disabled{};
  REQUIRE(model.randomize(disabled, [](auto& d, fuzzy::dsl::DslContext& ctx) {
    ctx.disable_soft(d.x == 1);
  }));
  CHECK(disabled.x == 2);

  A again{};
  REQUIRE(model.randomize(again));
  CHECK(again.x == 1);
}

TEST_CASE("disable_soft ignores nonmatching soft constraint") {
  struct A {
    int x{0};

    FUZZY_META(A, void, FUZZY_RAND(x))
    BLOCK(c) {
      require(x >= 0);
      require(x <= 10);
      soft(x == 1);
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize_with<A>([](auto& d, fuzzy::dsl::DslContext& ctx) {
    ctx.disable_soft(d.x == 2);
  });

  REQUIRE(r.has_value());
  CHECK(r->x == 1);
}
