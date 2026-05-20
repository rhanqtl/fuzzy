#include "catch2/catch_all.hpp"

#include "fuzzy/fuzzy.h"

TEST_CASE("inside discrete and span") {
  struct A {
    int x{0};

    FUZZY_META(A, void, FUZZY_RAND(x))
    BLOCK(b) {
      inside(x, 1, 3, fuzzy::dsl::inside_span(10, 12));
      require(x >= 0);
      require(x <= 20);
    }
    FUZZY_END
  };

  for (int i = 0; i < 30; ++i) {
    auto r = fuzzy::randomize<A>(static_cast<unsigned>(i + 99));
    REQUIRE(r.has_value());
    const int v = r->x;
    CHECK(((v == 1) || (v == 3) || (v >= 10 && v <= 12)));
  }
}

TEST_CASE("constraint_if applies then-branch only when condition holds") {
  struct A {
    int mode{0};
    int x{0};

    // `mode` is state (non-rand) so we can fix it per scenario like SV testbenches do with cfg fields.
    FUZZY_META(A, void, mode, FUZZY_RAND(x))
    BLOCK(b) {
      constraint_if(mode == 1, [&] { require(x == 42); });
      constraint_if(mode == 0, [&] { require(x == 7); });
    }
    FUZZY_END
  };

  A a0{};
  a0.mode = 0;
  fuzzy::Model<A> m0;
  REQUIRE(m0.randomize(a0));
  CHECK(a0.x == 7);

  A a1{};
  a1.mode = 1;
  fuzzy::Model<A> m1;
  REQUIRE(m1.randomize(a1));
  CHECK(a1.x == 42);
}

TEST_CASE("constraint_if_else") {
  struct A {
    int t{0};
    int x{0};

    FUZZY_META(A, void, FUZZY_RAND(t), FUZZY_RAND(x))
    BLOCK(b) {
      require(t >= 0);
      require(t <= 1);
      constraint_if_else(
          t == 0, [&] { require(x == 100); }, [&] { require(x == 200); });
    }
    FUZZY_END
  };

  A a{};
  REQUIRE(fuzzy::randomize_with(a, [](auto& d, fuzzy::dsl::DslContext& ctx) {
    ctx.require(d.t == 0);
  }));
  CHECK(a.x == 100);

  A b{};
  REQUIRE(fuzzy::randomize_with(b, [](auto& d, fuzzy::dsl::DslContext& ctx) {
    ctx.require(d.t == 1);
  }));
  CHECK(b.x == 200);
}

TEST_CASE("fuzzy_pre_randomize adjusts state before constraints") {
  struct A {
    int lo{5};
    int x{0};

    void fuzzy_pre_randomize() {
      lo = 40;
    }

    FUZZY_META(A, void, FUZZY_RAND(x), lo)
    BLOCK(b) {
      require(x >= lo);
      require(x <= 100);
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<A>();
  REQUIRE(r.has_value());
  CHECK(r->x >= 40);
}

TEST_CASE("fuzzy_post_randomize runs only after successful solve") {
  struct A {
    int x{0};
    int post_calls{0};

    void fuzzy_post_randomize() {
      ++post_calls;
    }

    FUZZY_META(A, void, FUZZY_RAND(x))
    BLOCK(b) {
      require(x == 0);
      require(x == 1);
    }
    FUZZY_END
  };

  A a{};
  fuzzy::Model<A> m;
  CHECK_FALSE(m.randomize(a));
  CHECK(a.post_calls == 0);
}

TEST_CASE("require false bool creates unsatisfiable constraint") {
  struct A {
    int x{0};

    FUZZY_META(A, void, FUZZY_RAND(x))
    BLOCK(b) {
      require(true);
      require(false);
    }
    FUZZY_END
  };

  CHECK_FALSE(fuzzy::randomize<A>().has_value());
}
