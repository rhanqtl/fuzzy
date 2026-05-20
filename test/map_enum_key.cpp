#include "catch2/catch_all.hpp"
#include "fuzzy/Dict.h"
#include "fuzzy/fuzzy.h"

enum class Color : int { Red = 0, Green = 1, Blue = 2 };

TEST_CASE("unordered_map with enum class key is solver-backed") {
  struct S {
    fuzzy::Dict<Color, int> m;

    FUZZY_META(S, void, FUZZY_RAND(m))
    BLOCK(c) {
      require(m.size() == 2);
      for_each_kv(m, [&](auto&, auto& k, auto& v) {
        fuzzy::Expr& ke = k;
        require(ke >= static_cast<int64_t>(0));
        require(ke <= static_cast<int64_t>(2));
        require(v == ke + static_cast<int64_t>(100));
      });
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<S>();
  REQUIRE(r.has_value());
  CHECK(r->m.size() == 2);
  for (const auto& [k, v] : r->m) {
    CHECK(v == static_cast<int>(k) + 100);
  }
  CHECK(fuzzy::validate(*r));
}

TEST_CASE("std::map with enum class key is solver-backed") {
  struct S {
    std::map<Color, int> m;

    FUZZY_META(S, void, FUZZY_RAND(m))
    BLOCK(c) {
      require(m.size() == 3);
      for_each_kv(m, [&](auto&, auto& k, auto& v) {
        fuzzy::Expr& ke = k;
        require(v == ke);
      });
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<S>();
  REQUIRE(r.has_value());
  CHECK(r->m.size() == 3);
  for (const auto& [k, v] : r->m) {
    CHECK(v == static_cast<int>(k));
  }
  CHECK(fuzzy::validate(*r));
}
