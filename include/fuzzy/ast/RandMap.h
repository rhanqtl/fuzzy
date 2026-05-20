#ifndef FUZZY_RAND_MAP_H
#define FUZZY_RAND_MAP_H

#include <cassert>
#include <cstddef>
#include <concepts>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "fuzzy/ast/Expr.h"
#include "fuzzy/ast/RandVar.h"

namespace fuzzy {

/// Map key: integer or `enum class` (stored / solved as underlying integer in Z3).
template <typename K>
concept RandMapKeyType = std::integral<K> || std::is_enum_v<K>;

template <typename V>
concept RandMapValueType = std::integral<V>;

namespace detail {

template <typename K>
struct RandMapKeyExpr {
  using type = VarExpr<K>;
};

template <typename K>
  requires std::is_enum_v<K>
struct RandMapKeyExpr<K> {
  using type = VarExprEnum<K>;
};

template <typename K>
using RandMapKeyExpr_t = typename RandMapKeyExpr<K>::type;

}  // namespace detail

class MapExprBase : public Expr {
 public:
  MapExprBase() :
      Expr{ExprKind::Var} {}

  virtual ~MapExprBase() = default;

  virtual VarExprBase& size_var_base() = 0;
  virtual std::size_t concrete_size() const = 0;
  virtual std::size_t actual_size() const = 0;

  virtual void materialize_entries(std::size_t n) = 0;
  virtual std::size_t num_entries() const = 0;
  virtual VarExprBase& key_var(std::size_t i) = 0;
  virtual VarExprBase& value_var(std::size_t i) = 0;

  // Symbolic key/value for nested for_each_kv (one pair per nesting level)
  virtual VarExprBase& symbolic_key() = 0;
  virtual VarExprBase& symbolic_value() = 0;

  virtual bool check_key_uniqueness() const = 0;
  virtual void write_back_entries() = 0;
};

template <typename MapT>
  requires RandMapKeyType<typename MapT::key_type> && RandMapValueType<typename MapT::mapped_type>
class BasicRandMap : public MapExprBase {
 public:
  using K = typename MapT::key_type;
  using V = typename MapT::mapped_type;
  using KeyExpr = detail::RandMapKeyExpr_t<K>;

  BasicRandMap(MapT& out, const std::string& name) :
      out_{out},
      name_{name},
      size_dummy_{out.size()},
      size_var_{size_dummy_, name + ".size"} {}

  VarExpr<std::size_t>& size() {
    return size_var_;
  }

  KeyExpr& symbolic_key_typed() {
    sym_keys_.push_back(
        std::make_unique<KeyExpr>(name_ + ".__symk" + std::to_string(sym_keys_.size())));
    return *sym_keys_.back();
  }

  VarExpr<V>& symbolic_value_typed() {
    sym_vals_.push_back(
        std::make_unique<VarExpr<V>>(name_ + ".__symv" + std::to_string(sym_vals_.size())));
    return *sym_vals_.back();
  }

  VarExprBase& symbolic_key() override {
    return symbolic_key_typed();
  }

  VarExprBase& symbolic_value() override {
    return symbolic_value_typed();
  }

  VarExprBase& size_var_base() override {
    return size_var_;
  }

  std::size_t concrete_size() const override {
    return size_dummy_;
  }

  std::size_t actual_size() const override {
    return out_.size();
  }

  void materialize_entries(std::size_t n) override {
    key_storage_.assign(n, K{});
    value_storage_.assign(n, V{});

    key_vars_.clear();
    value_vars_.clear();
    key_vars_.reserve(n);
    value_vars_.reserve(n);
    for (std::size_t i = 0; i < n; i++) {
      key_vars_.push_back(
          std::make_unique<KeyExpr>(key_storage_[i], name_ + ".key[" + std::to_string(i) + "]"));
      value_vars_.push_back(std::make_unique<VarExpr<V>>(value_storage_[i],
                                                          name_ + ".value[" + std::to_string(i) + "]"));
    }

    std::size_t i = 0;
    for (const auto& [k, v] : out_) {
      if (i >= n) {
        break;
      }
      key_storage_[i] = k;
      value_storage_[i] = v;
      i++;
    }
  }

  std::size_t num_entries() const override {
    assert(key_vars_.size() == value_vars_.size());
    return key_vars_.size();
  }

  VarExprBase& key_var(std::size_t i) override {
    assert(i < key_vars_.size());
    return *key_vars_[i];
  }

  VarExprBase& value_var(std::size_t i) override {
    assert(i < value_vars_.size());
    return *value_vars_[i];
  }

  bool check_key_uniqueness() const override {
    for (std::size_t i = 0; i < key_storage_.size(); i++) {
      for (std::size_t j = i + 1; j < key_storage_.size(); j++) {
        if (key_storage_[i] == key_storage_[j]) {
          return false;
        }
      }
    }
    return true;
  }

  void write_back_entries() override {
    out_.clear();
    if constexpr (requires(MapT m) { m.reserve(std::size_t{}); }) {
      out_.reserve(key_storage_.size());
    }
    for (std::size_t i = 0; i < key_storage_.size(); i++) {
      out_[key_storage_[i]] = value_storage_[i];
    }
    size_dummy_ = out_.size();
  }

 private:
  MapT& out_;
  std::string name_;

  std::size_t size_dummy_{0};
  VarExpr<std::size_t> size_var_;

  std::vector<K> key_storage_;
  std::vector<V> value_storage_;
  std::vector<std::unique_ptr<KeyExpr>> key_vars_;
  std::vector<std::unique_ptr<VarExpr<V>>> value_vars_;

  std::vector<std::unique_ptr<KeyExpr>> sym_keys_;
  std::vector<std::unique_ptr<VarExpr<V>>> sym_vals_;
};

template <typename K, typename V, typename Hash = std::hash<K>, typename Eq = std::equal_to<K>>
  requires RandMapKeyType<K> && RandMapValueType<V>
class RandMap : public BasicRandMap<std::unordered_map<K, V, Hash, Eq>> {
 public:
  using Base = BasicRandMap<std::unordered_map<K, V, Hash, Eq>>;
  RandMap(std::unordered_map<K, V, Hash, Eq>& out, const std::string& name) :
      Base(out, name) {}
};

template <typename K, typename V, typename Compare = std::less<K>,
          typename Alloc = std::allocator<std::pair<const K, V>>>
  requires RandMapKeyType<K> && RandMapValueType<V>
class RandOrderedMap : public BasicRandMap<std::map<K, V, Compare, Alloc>> {
 public:
  using Base = BasicRandMap<std::map<K, V, Compare, Alloc>>;
  RandOrderedMap(std::map<K, V, Compare, Alloc>& out, const std::string& name) :
      Base(out, name) {}
};
}  // namespace fuzzy

#endif  // FUZZY_RAND_MAP_H
