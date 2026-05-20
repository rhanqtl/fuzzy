#ifndef FUZZY_DICT_H
#define FUZZY_DICT_H

#include <functional>
#include <unordered_map>

namespace fuzzy {

/// 与 `std::unordered_map` 等价的一等别名（便于文档与 SV「关联数组」心智对齐）。
template <typename K, typename V, typename Hash = std::hash<K>, typename Eq = std::equal_to<K>,
          typename Alloc = std::allocator<std::pair<const K, V>>>
using Dict = std::unordered_map<K, V, Hash, Eq, Alloc>;

}  // namespace fuzzy

#endif
