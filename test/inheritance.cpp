#include "catch2/catch_all.hpp"

#include "fuzzy/fuzzy.h"

TEST_CASE("derived merges base constraints and uses base field short names") {
  struct Base {
    int x{0};

    FUZZY_META(Base, void, FUZZY_RAND(x))
    BLOCK(base_c) {
      require(x >= 0);
      require(x <= 10);
    }
    FUZZY_END
  };

  struct Derived : Base {
    int y{0};

    FUZZY_META(Derived, (Base), FUZZY_RAND(y))
    BLOCK(derived_c) {
      require(x > 5);
      require(y == x + 1);
    }
    FUZZY_END
  };

  auto result = fuzzy::randomize<Derived>();
  REQUIRE(result.has_value());
  CHECK(fuzzy::validate(*result));
  CHECK(result->x > 5);
  CHECK(result->x <= 10);
  CHECK(result->y == result->x + 1);
}

TEST_CASE("multiple bases each contribute constraints") {
  struct B1 {
    int a{0};
    FUZZY_META(B1, void, FUZZY_RAND(a))
    BLOCK(b1) { require(a >= 1); }
    FUZZY_END
  };

  struct B2 {
    int b{0};
    FUZZY_META(B2, void, FUZZY_RAND(b))
    BLOCK(b2) { require(b >= 2); }
    FUZZY_END
  };

  struct D : B1, B2 {
    int c{0};
    FUZZY_META(D, (B1, B2), FUZZY_RAND(c))
    BLOCK(d) {
      require(c == a + b);
    }
    FUZZY_END
  };

  auto result = fuzzy::randomize<D>();
  REQUIRE(result.has_value());
  CHECK(fuzzy::validate(*result));
  CHECK(result->a >= 1);
  CHECK(result->b >= 2);
  CHECK(result->c == result->a + result->b);
}

TEST_CASE("base root-scope constraints flush before derived named block") {
  struct Base {
    int x{0};
    FUZZY_META(Base, void, FUZZY_RAND(x))
    require(x == 7);
    BLOCK(base_named) { require(x >= 0); }
    FUZZY_END
  };

  struct Derived : Base {
    int y{0};
    FUZZY_META(Derived, (Base), FUZZY_RAND(y))
    BLOCK(only_derived) { require(y == 1); }
    FUZZY_END
  };

  auto result = fuzzy::randomize<Derived>();
  REQUIRE(result.has_value());
  CHECK(fuzzy::validate(*result));
  CHECK(result->x == 7);
  CHECK(result->y == 1);
}

TEST_CASE("derived same-name constraint block overrides base block") {
  struct Base {
    int x{0};

    FUZZY_META(Base, void, FUZZY_RAND(x))
    BLOCK(shared) { require(x == 1); }
    FUZZY_END
  };

  struct Derived : Base {
    int y{0};

    FUZZY_META(Derived, (Base), FUZZY_RAND(y))
    BLOCK(shared) { require(x == 2); }
    FUZZY_END
  };

  auto result = fuzzy::randomize<Derived>();
  REQUIRE(result.has_value());
  CHECK(result->x == 2);
  CHECK(fuzzy::validate(*result));
}

TEST_CASE("constraint_mode disabled derived override does not re-enable base block") {
  struct Base {
    int x{0};

    FUZZY_META(Base, void, FUZZY_RAND(x))
    BLOCK(shared) { require(x == 1); }
    FUZZY_END
  };

  struct Derived : Base {
    int y{0};

    FUZZY_META(Derived, (Base), FUZZY_RAND(y))
    BLOCK(shared) { require(x == 2); }
    FUZZY_END
  };

  Derived d{};
  d.fuzzy_constraint_mode("shared", 0);
  fuzzy::Model<Derived> model;
  REQUIRE(model.randomize(d));
  CHECK(model.check(d));
}
