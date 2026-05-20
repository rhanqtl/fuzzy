#include <map>
#include <unordered_map>

#include "catch2/catch_all.hpp"

#include "fuzzy/fuzzy.h"

TEST_CASE("unordered_map of integral key/value is solver-backed") {
  struct A {
    std::unordered_map<int, int> kv;

    FUZZY_META(A, void, FUZZY_RAND(kv))
      BLOCK(c) {
        require(kv.size() == 8);
      }
    FUZZY_END
  };

  auto result = fuzzy::randomize<A>();
  REQUIRE(result.has_value());
  CHECK(result->kv.size() == 8);
  CHECK(fuzzy::validate(*result));
}

TEST_CASE("ordered map of integral key/value is solver-backed") {
  struct A {
    std::map<int, int> kv;

    FUZZY_META(A, void, FUZZY_RAND(kv))
      BLOCK(c) {
        require(kv.size() == 6);
      }
    FUZZY_END
  };

  auto result = fuzzy::randomize<A>();
  REQUIRE(result.has_value());
  CHECK(result->kv.size() == 6);
  CHECK(fuzzy::validate(*result));
}

TEST_CASE("map size contradiction is unsat") {
  struct A {
    std::unordered_map<int, int> kv;

    FUZZY_META(A, void, FUZZY_RAND(kv))
      BLOCK(c) {
        require(kv.size() == 1);
        require(kv.size() == 2);
      }
    FUZZY_END
  };

  auto result = fuzzy::randomize<A>();
  CHECK_FALSE(result.has_value());
}

TEST_CASE("for_each_kv constrains each map entry") {
  struct A {
    std::unordered_map<int, int> kv;

    FUZZY_META(A, void, FUZZY_RAND(kv))
      BLOCK(c) {
        require(kv.size() == 5);
        for_each_kv(kv, [&](auto& i, auto& k, auto& v) {
          require(v == k + k);
        });
      }
    FUZZY_END
  };

  auto result = fuzzy::randomize<A>();
  REQUIRE(result.has_value());
  CHECK(fuzzy::validate(*result));
  for (const auto& [k, v] : result->kv) {
    CHECK(v == k + k);
  }
}
