#ifndef FUZZY_AST_CONTEXT_H
#define FUZZY_AST_CONTEXT_H

#include <ranges>
#include <vector>

namespace fuzzy {
template <typename T>
class RandVar;

template <typename T>
class RandArray;

template <typename K, typename V, typename Hash, typename Eq>
class RandMap;

template <typename T>
class Constant;

class Expr;
}  // namespace fuzzy

namespace fuzzy {
struct DebugInfo {
  std::string file;
  int line;
  std::string expr;
};
class ExprContext {
 public:
  template <typename T>
  RandVar<T>& create_var(T& out);

  template <typename T>
    requires std::ranges::random_access_range<T>
  RandArray<typename T::value_type>& create_array(T& out);

  template <typename T>
  RandMap<typename T::key_type, typename T::mapped_type, typename T::hasher, typename T::key_equal>&
  create_map(T& out);

  Constant<bool>& get_true();
  Constant<bool>& get_false();

  void add(Expr&, const DebugInfo&);

 public:
  const std::vector<Expr*>& roots() {
    return roots_;
  }

 private:
  std::vector<Expr*> roots_;
};
}  // namespace fuzzy

namespace fuzzy::detail {
extern thread_local ExprContext* curr_ctx;
}

#endif  // FUZZY_AST_CONTEXT_H
