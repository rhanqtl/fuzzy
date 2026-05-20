#include <algorithm>

#include "catch2/catch_all.hpp"

#include "fuzzy/fuzzy.h"

TEST_CASE("array_sum with fixed size") {
  struct A {
    std::vector<int> xs;

    FUZZY_META(A, void, FUZZY_RAND(xs))
    BLOCK(c) {
      require(xs.size() == 3);
      require(array_sum(xs) == 10);
      require(array_min(xs) == 1);
      require(array_max(xs) == 5);
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<A>();
  REQUIRE(r.has_value());
  CHECK(r->xs.size() == 3);
  int s = 0;
  int mn = r->xs[0];
  int mx = r->xs[0];
  for (int v : r->xs) {
    s += v;
    mn = std::min(mn, v);
    mx = std::max(mx, v);
  }
  CHECK(s == 10);
  CHECK(mn == 1);
  CHECK(mx == 5);
  CHECK(fuzzy::validate(*r));
}

TEST_CASE("array_sum empty is zero") {
  struct A {
    std::vector<int> xs;

    FUZZY_META(A, void, FUZZY_RAND(xs))
    BLOCK(c) {
      require(xs.size() == 0);
      require(array_sum(xs) == 0);
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<A>();
  REQUIRE(r.has_value());
  CHECK(r->xs.empty());
  CHECK(fuzzy::validate(*r));
}

TEST_CASE("array_min max with arithmetic") {
  struct A {
    std::vector<int> xs;

    FUZZY_META(A, void, FUZZY_RAND(xs))
    BLOCK(c) {
      require(xs.size() == 2);
      require(array_min(xs) + array_max(xs) == 7);
      require(array_min(xs) == 2);
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<A>();
  REQUIRE(r.has_value());
  CHECK(r->xs.size() == 2);
  CHECK(((r->xs[0] == 2 && r->xs[1] == 5) || (r->xs[0] == 5 && r->xs[1] == 2)));
  CHECK(fuzzy::validate(*r));
}

TEST_CASE("array_product and boolean reductions") {
  struct A {
    std::vector<int> xs;

    FUZZY_META(A, void, FUZZY_RAND(xs))
    BLOCK(c) {
      require(xs.size() == 3);
      for_each_i(xs, [&](auto&, auto& e) {
        require(e >= 0);
        require(e <= 3);
      });
      require(array_product(xs) == 6);
      require(array_and(xs) == 1);
      require(array_or(xs) == 1);
      require(array_xor(xs) == 1);
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<A>();
  REQUIRE(r.has_value());
  CHECK(r->xs.size() == 3);
  int product = 1;
  int truthy_count = 0;
  for (int v : r->xs) {
    product *= v;
    truthy_count += (v != 0);
  }
  CHECK(product == 6);
  CHECK(truthy_count == 3);
  CHECK((truthy_count % 2) == 1);
  CHECK(fuzzy::validate(*r));
}

TEST_CASE("empty array aggregate identities") {
  struct A {
    std::vector<int> xs;

    FUZZY_META(A, void, FUZZY_RAND(xs))
    BLOCK(c) {
      require(xs.size() == 0);
      require(array_product(xs) == 1);
      require(array_and(xs) == 1);
      require(array_or(xs) == 0);
      require(array_xor(xs) == 0);
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<A>();
  REQUIRE(r.has_value());
  CHECK(r->xs.empty());
  CHECK(fuzzy::validate(*r));
}

TEST_CASE("array reductions support with expression") {
  struct A {
    std::vector<int> xs;

    FUZZY_META(A, void, FUZZY_RAND(xs))
    BLOCK(c) {
      require(xs.size() == 3);
      for_each_i(xs, [&](auto&, auto& e) {
        require(e >= 1);
        require(e <= 3);
      });
      require(array_sum(xs, [](auto&, auto& e) -> fuzzy::Expr& { return e * 2; }) == 12);
      require(array_product(xs, [](auto&, auto& e) -> fuzzy::Expr& { return e + 1; }) == 24);
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<A>();
  REQUIRE(r.has_value());
  CHECK(r->xs.size() == 3);
  int doubled_sum = 0;
  int shifted_product = 1;
  for (int v : r->xs) {
    doubled_sum += v * 2;
    shifted_product *= v + 1;
  }
  CHECK(doubled_sum == 12);
  CHECK(shifted_product == 24);
  CHECK(fuzzy::validate(*r));
}

TEST_CASE("array boolean reductions support with expression") {
  struct A {
    std::vector<int> xs;

    FUZZY_META(A, void, FUZZY_RAND(xs))
    BLOCK(c) {
      require(xs.size() == 3);
      for_each_i(xs, [&](auto&, auto& e) {
        require(e >= 0);
        require(e <= 2);
      });
      require(array_and(xs, [](auto&, auto& e) -> fuzzy::Expr& { return e > 0; }) == 0);
      require(array_or(xs, [](auto&, auto& e) -> fuzzy::Expr& { return e == 2; }) == 1);
      require(array_xor(xs, [](auto&, auto& e) -> fuzzy::Expr& { return e == 2; }) == 1);
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<A>();
  REQUIRE(r.has_value());
  int eq_two = 0;
  bool all_positive = true;
  for (int v : r->xs) {
    all_positive = all_positive && (v > 0);
    eq_two += (v == 2);
  }
  CHECK_FALSE(all_positive);
  CHECK((eq_two % 2) == 1);
  CHECK(fuzzy::validate(*r));
}
