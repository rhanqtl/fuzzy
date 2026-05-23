#include "catch2/catch_all.hpp"
#include "fuzzy/fuzzy.h"
#include "fuzzy/Model.h"

TEST_CASE("Model seed call_count next_seed set_seed") {
  struct A {
    int x{0};
    FUZZY_META(A, void, FUZZY_RAND(x))
    BLOCK(b) { require(x >= 0 && x <= 1); }
    FUZZY_END
  };

  fuzzy::Model<A> m(100u);
  CHECK(m.seed() == 100u);
  CHECK(m.call_count() == 0u);
  CHECK(m.next_seed() == 100u);
  A a{};
  REQUIRE(m.randomize(a));
  CHECK(m.call_count() == 1u);
  CHECK(m.next_seed() == 101u);
  m.set_seed(200u);
  CHECK(m.seed() == 200u);
  CHECK(m.call_count() == 0u);
}

TEST_CASE("randomize optional factory with seed") {
  struct A {
    int x{0};
    FUZZY_META(A, void, FUZZY_RAND(x))
    BLOCK(b) { require(x == 3); }
    FUZZY_END
  };
  auto r = fuzzy::randomize<A>(7u);
  REQUIRE(r.has_value());
  CHECK(r->x == 3);
}

TEST_CASE("validate free function") {
  struct A {
    int x{1};
    FUZZY_META(A, void, FUZZY_RAND(x))
    BLOCK(b) { require(x == 1); }
    FUZZY_END
  };
  A a{};
  CHECK(fuzzy::validate(a));
  CHECK(fuzzy::check(a));
}

TEST_CASE("Model check is current-value validation without randomizing") {
  struct A {
    int x{1};
    FUZZY_META(A, void, FUZZY_RAND(x))
    BLOCK(b) { require(x == 1); }
    FUZZY_END
  };

  fuzzy::Model<A> model;
  A a{};
  CHECK(model.validate(a));
  a.x = 2;
  CHECK_FALSE(model.validate(a));
  CHECK(a.x == 2);
}

TEST_CASE("Model rand_state saves and restores seed position") {
  struct A {
    int x{0};
    FUZZY_META(A, void, FUZZY_RAND(x))
    BLOCK(b) {
      require(x >= 0);
      require(x <= 100);
    }
    FUZZY_END
  };

  fuzzy::Model<A> model(19u);
  A first{};
  REQUIRE(model.randomize(first));
  auto state = model.rand_state();
  A second{};
  REQUIRE(model.randomize(second));
  model.set_rand_state(state);
  A replay{};
  REQUIRE(model.randomize(replay));
  CHECK(replay.x == second.x);
}

TEST_CASE("Model randomize variable list pins omitted rand fields") {
  struct A {
    int x{3};
    int y{4};

    FUZZY_META(A, void, FUZZY_RAND(x), FUZZY_RAND(y))
    BLOCK(c) {
      require(x + y == 10);
      require(x >= 0);
      require(y >= 0);
    }
    FUZZY_END
  };

  fuzzy::Model<A> model(5u);
  A a{};
  REQUIRE(model.randomize(a, {"x"}));
  CHECK(a.x == 6);
  CHECK(a.y == 4);
  CHECK(model.validate(a));
}

TEST_CASE("Model randomize empty variable list only checks current state") {
  struct A {
    int x{1};
    int y{2};

    FUZZY_META(A, void, FUZZY_RAND(x), FUZZY_RAND(y))
    BLOCK(c) { require(x + y == 3); }
    FUZZY_END
  };

  fuzzy::Model<A> model(5u);
  A ok{};
  REQUIRE(model.randomize(ok, {}));
  CHECK(ok.x == 1);
  CHECK(ok.y == 2);

  A bad{};
  bad.y = 3;
  CHECK_FALSE(model.randomize(bad, {}));
  CHECK(bad.x == 1);
  CHECK(bad.y == 3);
}

TEST_CASE("randomize_with variable list combines pins and inline constraints") {
  struct A {
    int x{0};
    int y{5};

    FUZZY_META(A, void, FUZZY_RAND(x), FUZZY_RAND(y))
    BLOCK(c) {
      require(x >= 0);
      require(y >= 0);
    }
    FUZZY_END
  };

  A a{};
  REQUIRE(fuzzy::randomize_with(a, {"x"}, [](auto& d, fuzzy::dsl::DslContext& ctx) {
    ctx.require(d.x + d.y == 9);
  }));
  CHECK(a.x == 4);
  CHECK(a.y == 5);
}

TEST_CASE("variable list pins dynamic array size and elements") {
  struct A {
    int x{0};
    std::vector<int> xs{1, 2};

    FUZZY_META(A, void, FUZZY_RAND(x), FUZZY_RAND(xs))
    BLOCK(c) {
      require(xs.size() == 2);
      require(array_sum(xs) + x == 10);
    }
    FUZZY_END
  };

  A a{};
  fuzzy::Model<A> model(5u);
  REQUIRE(model.randomize(a, {"x"}));
  CHECK(a.x == 7);
  REQUIRE(a.xs.size() == 2);
  CHECK(a.xs[0] == 1);
  CHECK(a.xs[1] == 2);
}

TEST_CASE("randomize_scope randomizes local scalar variables with inline constraints") {
  int x{0};
  int y{0};

  REQUIRE(fuzzy::randomize_scope(x, y).with(
      11u, [](fuzzy::dsl::DslContext& ctx, auto& sx, auto& sy) {
        ctx.require(sx >= 0);
        ctx.require(sy >= 0);
        ctx.require(sx + sy == 9);
        ctx.require(sx < sy);
      }));

  CHECK(x >= 0);
  CHECK(y >= 0);
  CHECK(x + y == 9);
  CHECK(x < y);
}
