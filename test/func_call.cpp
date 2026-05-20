#include "catch2/catch_all.hpp"

#include "fuzzy/fuzzy.h"

// 成员函数作为约束中的可调用：`invoke` / `FUZZY_CALL`（与 `test/planner/multi_steps.cpp` 互补：此处为单成员调用链）

struct Equation {
  int x{};
  int y{};
  int z{};

  int foo(int v) const {
    return v + 1;
  }

  FUZZY_META(Equation, void, FUZZY_RAND(x), FUZZY_RAND(y), FUZZY_RAND(z))
  auto Foo = FUZZY_DECLARE_FUNC(&Equation::foo);
  BLOCK(c) {
    require(x >= 0);
    require(x <= 5);
    require(y >= 0);
    require(y <= 5);
    require(z >= 0);
    require(z <= 5);
    require(x + y == invoke(__obj, Foo, z));
  }
  FUZZY_END
};

TEST_CASE("invoke: member function in arithmetic constraint") {
  auto r = fuzzy::randomize<Equation>();
  REQUIRE(r.has_value());
  CHECK(r->x + r->y == r->foo(r->z));
  CHECK(fuzzy::validate(*r));
}
