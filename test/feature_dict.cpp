#include "catch2/catch_all.hpp"
#include "fuzzy/Dict.h"
#include "fuzzy/fuzzy.h"

TEST_CASE("fuzzy::Dict alias works with FUZZY_RAND and for_each_kv") {
  struct S {
    fuzzy::Dict<int, int> kv;

    FUZZY_META(S, void, FUZZY_RAND(kv))
    BLOCK(c) {
      require(kv.size() == 3);
      for_each_kv(kv, [&](auto&, auto& k, auto& v) {
        fuzzy::Expr& ke = k;
        require(v == ke + ke + ke + ke + ke + ke + ke + ke + ke + ke);
      });
    }
    FUZZY_END
  };

  auto r = fuzzy::randomize<S>();
  REQUIRE(r.has_value());
  CHECK(r->kv.size() == 3);
  for (const auto& [k, v] : r->kv) {
    CHECK(v == k * 10);
  }
  CHECK(fuzzy::validate(*r));
}
