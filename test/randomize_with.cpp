#include "catch2/catch_all.hpp"

#include "fuzzy/fuzzy.h"

TEST_CASE("randomize_with pins scalar for one call") {
  struct A {
    int x{0};

    FUZZY_META(A, void, FUZZY_RAND(x))
    BLOCK(b) {
      require(x >= 0);
      require(x <= 100);
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize_with<A>([](auto& d, fuzzy::dsl::DslContext& ctx) {
    ctx.require(d.x == 42);
  });
  REQUIRE(r.has_value());
  CHECK(r->x == 42);
  CHECK(fuzzy::validate(*r));
}

TEST_CASE("randomize_with on existing object via Model") {
  struct A {
    int x{0};

    FUZZY_META(A, void, FUZZY_RAND(x))
    BLOCK(b) {
      require(x >= 0);
      require(x <= 10);
    }
    FUZZY_END
  };

  A a{};
  fuzzy::Model<A> m;
  REQUIRE(m.randomize(a, [](auto& d, fuzzy::dsl::DslContext& ctx) { ctx.require(d.x == 7); }));
  CHECK(a.x == 7);
  CHECK(fuzzy::validate(a));
}

TEST_CASE("randomize_with fails when contradicting hard constraints") {
  struct A {
    int x{0};

    FUZZY_META(A, void, FUZZY_RAND(x))
    BLOCK(b) { require(x <= 5); }
    FUZZY_END
  };

  A a{};
  fuzzy::Model<A> m;
  CHECK_FALSE(m.randomize(a, [](auto& d, fuzzy::dsl::DslContext& ctx) { ctx.require(d.x == 99); }));
}
