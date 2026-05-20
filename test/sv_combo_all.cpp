#include <algorithm>

#include "catch2/catch_all.hpp"
#include "fuzzy/fuzzy.h"
#include "fuzzy/Model.h"

TEST_CASE("combo inside implies constraint_if array_agg unique for_each soft solve_before with") {
  struct A {
    int mode{0};
    int cap{10};
    std::vector<int> xs;

    void fuzzy_pre_randomize() {
      cap = 12;
    }

    FUZZY_META(A, void, mode, FUZZY_RAND(xs), cap)
    BLOCK(main) {
      inside(mode, 0, 1);
      require(xs.size() == 3);
      unique(xs);
      for_each_i(xs, [&](auto&, auto& e) { require(e >= 0); });
      require(array_sum(xs) == cap);
      constraint_if(mode == 0, [&] { require(array_min(xs) == 1); });
      constraint_if(mode == 1, [&] { require(array_max(xs) == cap - 2); });
      require(implies(mode == 0, xs.size() >= 0));
      soft(xs.size() > 0);
      solve_before(cap, xs.size());
    }
    FUZZY_END
  };

  A a0{};
  a0.mode = 0;
  fuzzy::Model<A> m0(11u);
  REQUIRE(m0.randomize(a0));
  CHECK(fuzzy::validate(a0));
  CHECK(a0.xs.size() == 3);
  const int s0 = a0.xs[0] + a0.xs[1] + a0.xs[2];
  CHECK(s0 == a0.cap);
  CHECK(*std::min_element(a0.xs.begin(), a0.xs.end()) == 1);

  A a1{};
  a1.mode = 1;
  REQUIRE(fuzzy::randomize_with(a1, 22u, [](auto& d, fuzzy::dsl::DslContext& ctx) {
    ctx.require(d.mode == 1);
    ctx.require(ctx.array_sum(d.xs) == d.cap);
  }));
  CHECK(fuzzy::validate(a1));
  CHECK(*std::max_element(a1.xs.begin(), a1.xs.end()) == a1.cap - 2);
}

TEST_CASE("combo dist unique constraint_mode disables optional block only") {
  struct A {
    int x{0};
    std::vector<int> xs;
    FUZZY_META(A, void, FUZZY_RAND(x), FUZZY_RAND(xs))
    BLOCK(core) {
      require(xs.size() == 2);
      unique(xs);
    }
    BLOCK(optional_dist) {
      dist(x, {{0, 1}, {1, 1}});
    }
    FUZZY_END
  };

  A a{};
  a.fuzzy_constraint_mode("optional_dist", 0);
  fuzzy::Model<A> m;
  REQUIRE(m.randomize(a));
  CHECK(a.xs.size() == 2);
  CHECK(fuzzy::validate(a));
  a.fuzzy_constraint_mode("optional_dist", 1);
}
