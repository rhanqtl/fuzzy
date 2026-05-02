#include "catch2/catch_all.hpp"

#include "fuzzy/Model.h"
#include "fuzzy/fuzzy.h"

namespace {
int plus_one(int v) {
  return v + 1;
}

int add_three(int v) {
  return v + 3;
}
}  // namespace

struct Equation {
  int x;
  std::vector<int> ys;

  auto neg(int x) const {
    return -x;
  }

  FUZZY_META(Equation, x, ys)
    auto Neg = FUZZY_DECLARE_FUNC(&Equation::neg);
    auto PlusOne = FUZZY_DECLARE_FUNC(&plus_one);
    BLOCK(c) {
      constrain(x < 0);
      constrain(ys.size() == FUZZY_CALL(__obj, PlusOne, FUZZY_CALL(__obj, Neg, x)));
      constrain(ys.size() == invoke(__obj, PlusOne, invoke(__obj, Neg, x)));
    }
  FUZZY_END
};

TEST_CASE("planner/multi-steps") {
  auto result = fuzzy::randomize<Equation>();
  REQUIRE(result.has_value());
  const auto x = result->x;
  const auto &ys = result->ys;
  const auto neg_x = result->neg(x);
  const auto expected_size = plus_one(neg_x);
  CHECK(x < 0);
  CHECK(ys.size() == static_cast<std::size_t>(expected_size));
  CHECK(fuzzy::validate(*result));
}

struct NestedEquation {
  int x;
  std::vector<int> ys;

  int times_two(int v) const {
    return 2 * v;
  }

  FUZZY_META(NestedEquation, x, ys)
    auto TimesTwo = FUZZY_DECLARE_FUNC(&NestedEquation::times_two);
    auto AddThree = FUZZY_DECLARE_FUNC(&add_three);
    BLOCK(c) {
      constrain(x >= 0);
      constrain(x <= 10);
      constrain(ys.size() == FUZZY_CALL(__obj, AddThree, FUZZY_CALL(__obj, TimesTwo, x)));
    }
  FUZZY_END
};

TEST_CASE("planner/multi-steps/nested-call") {
  auto result = fuzzy::randomize<NestedEquation>();
  REQUIRE(result.has_value());

  const auto x = result->x;
  const auto expected_size = add_three(result->times_two(x));
  CHECK(x >= 0);
  CHECK(x <= 10);
  CHECK(result->ys.size() == static_cast<std::size_t>(expected_size));
  CHECK(fuzzy::validate(*result));
}
