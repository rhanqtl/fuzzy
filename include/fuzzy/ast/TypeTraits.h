#ifndef FUZZY_AST_TYPE_TRAITS_H
#define FUZZY_AST_TYPE_TRAITS_H

#include <unordered_map>
#include <vector>

#include "fuzzy/ast/RandArray.h"
#include "fuzzy/ast/RandMap.h"
#include "fuzzy/ast/RandString.h"
#include "fuzzy/ast/RandVar.h"

namespace fuzzy {
template <typename...>
struct convert_to_rand;

template <typename T>
struct convert_to_rand<T> {
  using type = RandVar<T>;
};

template <>
struct convert_to_rand<std::string> {
  using type = RandString;
};

template <typename K, typename V, typename Hash /*= std::hash<K>*/,
          typename Eq /*= std::equal_to<K>, typename...*/>
struct convert_to_rand<std::unordered_map<K, V, Hash, Eq>> {
  using type = RandMap<K, V, Hash, Eq>;
};

template <typename T>
struct convert_to_rand<std::vector<T>> {
  using type = RandArray<T>;
};

template <typename... Ts>
using convert_to_rand_t = typename convert_to_rand<Ts...>::type;
}  // namespace fuzzy

#endif  // FUZZY_AST_TYPE_TRAITS_H
