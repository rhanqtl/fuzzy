#ifndef FUZZY_RAND_MAP_H
#define FUZZY_RAND_MAP_H

#include <cstddef>
#include <functional>
#include <memory>
#include <utility>

#include "fuzzy/ast/Expr.h"
#include "fuzzy/ast/RandVar.h"

namespace fuzzy {
template <typename K, typename V, typename Hash = std::hash<K>, typename Eq = std::equal_to<K>>
class RandMap : public Expr {
 public:
  RandVar<std::size_t>& length() {
    return len_var_;
  }

  template <typename F>
  void with(F&& fn) {
    thunk_ = std::forward<F>(fn);
  }

 private:
  RandVar<std::size_t> len_var_;
  std::size_t len_{0};
  using Entry = std::pair<std::reference_wrapper<RandVar<K>>, std::reference_wrapper<RandVar<V>>>;
  std::unique_ptr<Entry[]> kv_pairs_;

  std::function<void(RandVar<K>&, RandVar<V>&)> thunk_;
};
}  // namespace fuzzy

#endif  // FUZZY_RAND_MAP_H
