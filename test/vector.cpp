#include <unordered_set>
#include <vector>
#include "catch2/catch_all.hpp"

#include "fuzzy/fuzzy.h"

TEST_CASE("vector of built-in types") {
  struct A {
    std::vector<int> xs;
  
    FUZZY_META(A, void, FUZZY_RAND(xs))
      BLOCK(c) {
        require(xs.size() == 10);
        unique(xs);
      }
    FUZZY_END
  };

  auto result = fuzzy::randomize<A>();
  REQUIRE(result.has_value());
  CHECK(fuzzy::validate(*result));
  const auto &xs = result->xs;
  std::unordered_set<int> dedup{xs.begin(), xs.end()};
  CHECK(dedup.size() == xs.size());
}

TEST_CASE("vector of user-defined types is not a supported random array") {
  struct T {
    int data{0};

    FUZZY_META(T, void, FUZZY_RAND(data))
    FUZZY_END
  };

  struct A {
    std::vector<T> xs;

    FUZZY_META(A, void, FUZZY_RAND(xs))
      BLOCK(c) {
        require(xs.size() == 10);
        for_each_i(xs, [&](auto &i, auto &item) {
          require(item.data == i);
        });
      }
    FUZZY_END
  };

  auto result = fuzzy::randomize<A>();
  CHECK_FALSE(result.has_value());
}
