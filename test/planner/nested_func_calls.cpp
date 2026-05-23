#include "catch2/catch_all.hpp"

#include "fuzzy/Model.h"
#include "fuzzy/fuzzy.h"

// L1 规则 1.a：a == f(g(h(b))) 应先解 b，再由内向外归约 h→g→f，最后解 a。
namespace {

int h(int v) {
  return v + 1;
}

int g(int v) {
  return v * 2;
}

int f(int v) {
  return v - 3;
}

struct NestedFuncCalls {
  int a{0};
  int b{0};

  FUZZY_META(NestedFuncCalls, void, FUZZY_RAND(a), FUZZY_RAND(b))
  auto F = FUZZY_DECLARE_FUNC(&f);
  auto G = FUZZY_DECLARE_FUNC(&g);
  auto H = FUZZY_DECLARE_FUNC(&h);
  BLOCK(c) {
    require(b >= 0);
    require(b <= 5);
    require(a == invoke(__obj, F, invoke(__obj, G, invoke(__obj, H, b))));
  }
  FUZZY_END
};

}  // namespace

TEST_CASE("planner/nested-function-calls") {
  auto result = fuzzy::randomize<NestedFuncCalls>(37u);
  REQUIRE(result.has_value());

  const auto b = result->b;
  const auto expected_a = f(g(h(b)));
  CHECK(b >= 0);
  CHECK(b <= 5);
  CHECK(result->a == expected_a);
  CHECK(fuzzy::validate(*result));
}
