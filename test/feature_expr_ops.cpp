#include "catch2/catch_all.hpp"
#include "fuzzy/fuzzy.h"

/// Exercise every `Expr.cpp` scalar operator overload in one satisfiable model.
TEST_CASE("all Expr binary/unary overloads used in constraints") {
  struct A {
    int x{0};
    int y{0};
    FUZZY_META(A, void, FUZZY_RAND(x), FUZZY_RAND(y))
    BLOCK(c) {
      require(x + y == 3);
      require(x - y == -1);
      require(x < y);
      require(x <= y);
      require(y >= x);
      require(x <= 1);
      require(y >= 2);
      require(y > x);
      require(y > 1);
      require(x == 1);
      require(y == 2);
      require(x != y);
      require(!(x == 0));
      require(abs(x - y) == 1);
      require(1 + x == 2);
      require(x + 1 == 2);
      require(2 - x == 1);
      require(x - 1 == 0);
      require(1 < y);
      require(x < 2);
      require(1 <= y);
      require(x <= 1);
      require(2 > x);
      require(y > 1);
      require(2 >= y);
      require(y >= 2);
      require(1 == x);
      require(x == 1);
      require(0 != y);
      require(y != 1);
      require((x < y) && (x + y > 0));
      require((x == 1) || (y == 99));
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<A>();
  REQUIRE(r.has_value());
  CHECK(r->x == 1);
  CHECK(r->y == 2);
  CHECK(fuzzy::validate(*r));
}

TEST_CASE("integer bit helpers cover select slice concat and wildcard match") {
  struct A {
    int x{0};
    int hi{0};
    int lo{0};
    int joined{0};

    FUZZY_META(A, void, FUZZY_RAND(x), FUZZY_RAND(hi), FUZZY_RAND(lo), FUZZY_RAND(joined))
    BLOCK(c) {
      require(x >= 0);
      require(x <= 15);
      require(bit_select(x, 2) == 0);
      require(bit_slice(x, 3, 1) == 5);
      require(wildcard_eq(x, 0b1010, 0b1110));
      require(wildcard_ne(x, 0b0010, 0b1110));

      require(hi == 2);
      require(lo == 3);
      require(joined == bit_concat(hi, 2, lo));
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<A>();
  REQUIRE(r.has_value());
  CHECK(r->x >= 0);
  CHECK(r->x <= 15);
  CHECK(((r->x >> 2) & 1) == 0);
  CHECK(((r->x >> 1) & 0b111) == 5);
  CHECK((r->x & 0b1110) == 0b1010);
  CHECK((r->x & 0b1110) != 0b0010);
  CHECK(r->joined == 11);
  CHECK(fuzzy::validate(*r));
}
