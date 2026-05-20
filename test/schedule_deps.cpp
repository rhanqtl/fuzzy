#include <cstddef>
#include <unordered_map>
#include <vector>

#include "catch2/catch_all.hpp"
#include "fuzzy/fuzzy.h"

// Scheduling dependency tests (L1 unidirectional edges).
// L0 (hooks, RNG, rand_mode, var_list pin) is out of scope here.
//
// 1.a  Call arguments before call reduction.
// 1.b  solve_before(lhs, rhs) => lhs before rhs.
// 1.c  container size before elements / foreach expansion.

namespace {

int triple(int v) {
  return 3 * v;
}

int plus_two(int v) {
  return v + 2;
}

}  // namespace

// ---- 1.a function call: arguments before call result is substituted ----

TEST_CASE("schedule/call/argument-used-in-call-then-lhs") {
  struct A {
    int a{0};
    int b{0};

    int inc(int v) const {
      return v + 1;
    }

    FUZZY_META(A, void, FUZZY_RAND(a), FUZZY_RAND(b))
    auto Inc = FUZZY_DECLARE_FUNC(&A::inc);
    BLOCK(c) {
      require(b >= 0);
      require(b <= 5);
      require(a == invoke(__obj, Inc, b));
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<A>(11u);
  REQUIRE(r.has_value());
  CHECK(r->a == r->inc(r->b));
  CHECK(fuzzy::validate(*r));
}

TEST_CASE("schedule/call/nested-calls-inner-argument-first") {
  struct A {
    int x{0};
    std::vector<int> ys;

    FUZZY_META(A, void, FUZZY_RAND(x), FUZZY_RAND(ys))
    auto Triple = FUZZY_DECLARE_FUNC(&triple);
    auto PlusTwo = FUZZY_DECLARE_FUNC(&plus_two);
    BLOCK(c) {
      require(x >= 0);
      require(x <= 4);
      require(ys.size() == invoke(__obj, PlusTwo, invoke(__obj, Triple, x)));
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<A>(19u);
  REQUIRE(r.has_value());
  const int expected = plus_two(triple(r->x));
  CHECK(expected >= 0);
  CHECK(r->ys.size() == static_cast<std::size_t>(expected));
  CHECK(fuzzy::validate(*r));
}

// ---- 1.b solve_before ----

TEST_CASE("schedule/solve-before/coupled-scalars-stay-consistent") {
  struct A {
    int a{0};
    int b{0};

    FUZZY_META(A, void, FUZZY_RAND(a), FUZZY_RAND(b))
    BLOCK(c) {
      require(a >= 0);
      require(a <= 50);
      require(b >= 1);
      require(b <= 51);
      require(b == a + 1);
      solve_before(a, b);
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<A>(42u);
  REQUIRE(r.has_value());
  CHECK(r->b == r->a + 1);
  CHECK(fuzzy::validate(*r));
}

TEST_CASE("schedule/solve-before/rejects-randc-in-ordering") {
  struct A {
    int a{0};
    int b{0};

    FUZZY_META(A, void, FUZZY_RANDC(a), FUZZY_RAND(b))
    BLOCK(c) {
      require(a >= 0);
      require(a <= 2);
      require(b >= 0);
      require(b <= 2);
      solve_before(a, b);
    }
    FUZZY_END
  };

  CHECK_FALSE(fuzzy::randomize<A>().has_value());
}

// ---- 1.c size before element / foreach expansion ----

TEST_CASE("schedule/size-before/foreach-binds-each-element") {
  struct A {
    std::vector<int> xs;

    FUZZY_META(A, void, FUZZY_RAND(xs))
    BLOCK(c) {
      require(xs.size() == 4);
      for_each_i(xs, [&](auto& i, auto& e) {
        require(e == i + 1);
      });
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<A>(7u);
  REQUIRE(r.has_value());
  REQUIRE(r->xs.size() == 4);
  for (std::size_t i = 0; i < r->xs.size(); ++i) {
    CHECK(r->xs[i] == static_cast<int>(i) + 1);
  }
  CHECK(fuzzy::validate(*r));
}

TEST_CASE("schedule/size-before/zero-size-skips-element-constraints") {
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

TEST_CASE("schedule/size-before/map-kv-after-size") {
  struct A {
    std::unordered_map<int, int> kv;

    FUZZY_META(A, void, FUZZY_RAND(kv))
    BLOCK(c) {
      require(kv.size() == 3);
      for_each_kv(kv, [&](auto&, auto& k, auto& v) {
        require(v == k * 10);
      });
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<A>(3u);
  REQUIRE(r.has_value());
  REQUIRE(r->kv.size() == 3);
  for (const auto& [k, v] : r->kv) {
    CHECK(v == k * 10);
  }
  CHECK(fuzzy::validate(*r));
}

// ---- combined chain: call -> array size -> foreach on elements ----

TEST_CASE("schedule/combined/call-then-size-then-foreach-elements") {
  struct A {
    int x{0};
    std::vector<int> ys;

    int width(int v) const {
      return v + 1;
    }

    FUZZY_META(A, void, FUZZY_RAND(x), FUZZY_RAND(ys))
    auto Width = FUZZY_DECLARE_FUNC(&A::width);
    BLOCK(c) {
      require(x >= 0);
      require(x <= 3);
      require(ys.size() == invoke(__obj, Width, x));
      for_each_i(ys, [&](auto&, auto& e) {
        require(e >= 0);
        require(e <= 9);
      });
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<A>(13u);
  REQUIRE(r.has_value());
  const std::size_t expected = static_cast<std::size_t>(r->width(r->x));
  CHECK(r->ys.size() == expected);
  for (int v : r->ys) {
    CHECK(v >= 0);
    CHECK(v <= 9);
  }
  CHECK(fuzzy::validate(*r));
}
