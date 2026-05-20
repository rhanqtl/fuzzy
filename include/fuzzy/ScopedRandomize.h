#ifndef FUZZY_SCOPED_RANDOMIZE_H
#define FUZZY_SCOPED_RANDOMIZE_H

#include <optional>
#include <random>
#include <string>
#include <tuple>
#include <utility>

#include "fuzzy/ConstraintCollector.h"
#include "fuzzy/SolveStrategy.h"
#include "fuzzy/dsl/Context.h"

namespace fuzzy {

namespace detail {

template <typename... Ts>
class ScopedRandomizer {
 public:
  explicit ScopedRandomizer(Ts&... vars) :
      vars_{vars...} {}

  template <typename WithFn>
  bool with(WithFn&& with_fn) {
    return randomize(std::random_device{}(), std::forward<WithFn>(with_fn));
  }

  template <typename WithFn>
  bool with(unsigned seed, WithFn&& with_fn) {
    return randomize(seed, std::forward<WithFn>(with_fn));
  }

  template <typename WithFn>
  bool randomize(unsigned seed, WithFn&& with_fn) {
    ConstraintCollector cc;
    auto proxies = make_proxies(cc, std::index_sequence_for<Ts...>{});
    dsl::DslContext ctx{cc};
    std::apply(
        [&](auto&... proxy_refs) {
          std::forward<WithFn>(with_fn)(ctx, proxy_refs...);
        },
        proxies);
    cc.finalize();
    SolveStrategy strategy;
    return strategy.solve(cc, seed);
  }

 private:
  template <std::size_t... Is>
  auto make_proxies(ConstraintCollector& cc, std::index_sequence<Is...>) {
    return std::tuple<decltype(cc.create_proxy(std::get<Is>(vars_), "__scope" + std::to_string(Is),
                                               CompileRandKind::Rand, static_cast<bool*>(nullptr)))&...>{
        cc.create_proxy(std::get<Is>(vars_), "__scope" + std::to_string(Is), CompileRandKind::Rand,
                        static_cast<bool*>(nullptr))...};
  }

  std::tuple<Ts&...> vars_;
};

}  // namespace detail

/// C++ analogue of `std::randomize(var_list) with { ... }` for scalar local variables.
/// Use `randomize_scope(x, y).with(seed, [](ctx, sx, sy) { ... })`.
template <typename... Ts>
detail::ScopedRandomizer<Ts...> randomize_scope(Ts&... vars) {
  return detail::ScopedRandomizer<Ts...>{vars...};
}

}  // namespace fuzzy

#endif  // FUZZY_SCOPED_RANDOMIZE_H
