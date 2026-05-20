#ifndef FUZZY_AST_RAND_VAR_H
#define FUZZY_AST_RAND_VAR_H

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#include "fuzzy/ast/Expr.h"

namespace fuzzy {

/// Compile-time randomness kind (SystemVerilog `rand` / `randc` / state).
enum class CompileRandKind { Rand, Randc, State };

/// SystemVerilog `randc`：当前排列周期内已出现过的取值，直到可行域耗尽后清空再开始新周期。
class RandcCycle {
 public:
  [[nodiscard]] const std::vector<int64_t>& used_values() const noexcept {
    return used_in_permutation_;
  }

  void clear_permutation() noexcept {
    used_in_permutation_.clear();
  }

  void record_draw(int64_t v) {
    used_in_permutation_.push_back(v);
  }

 private:
  std::vector<int64_t> used_in_permutation_;
};

// Base class for type-erased variable access (used by solver)
class VarExprBase : public Expr {
 public:
  VarExprBase(std::string name, CompileRandKind compile_kind, bool* rand_mode_ptr,
              RandcCycle* randc_cycle = nullptr) :
      Expr{ExprKind::Var},
      name_{std::move(name)},
      compile_kind_{compile_kind},
      rand_mode_ptr_{rand_mode_ptr},
      randc_cycle_{randc_cycle} {}

  const std::string& name() const {
    return name_;
  }

  CompileRandKind compile_kind() const {
    return compile_kind_;
  }

  [[nodiscard]] RandcCycle* randc_cycle() const noexcept {
    return randc_cycle_;
  }

  /// Runtime randomization enable (SV `rand_mode`). Ignored when `compile_kind() == State`.
  bool rand_mode_enabled() const {
    if (compile_kind_ == CompileRandKind::State) {
      return false;
    }
    if (!rand_mode_ptr_) {
      return true;
    }
    return *rand_mode_ptr_;
  }

  /// If false, the solver must treat this variable as fixed to `read_as_int64()`.
  bool is_decision_var_for_solve() const {
    if (is_symbolic()) {
      return true;
    }
    if (compile_kind_ == CompileRandKind::State) {
      return false;
    }
    return rand_mode_enabled();
  }

  // Type-erased write-back from int64_t (Z3 result)
  virtual void write_back(int64_t value) = 0;

  // Type-erased read of the current value as int64_t
  virtual int64_t read_as_int64() const = 0;

  // Whether this is a symbolic variable (not bound to real output)
  virtual bool is_symbolic() const = 0;

 private:
  std::string name_;
  CompileRandKind compile_kind_;
  bool* rand_mode_ptr_;
  RandcCycle* randc_cycle_;
};

// Concrete typed variable expression
template <std::integral T>
class VarExpr : public VarExprBase {
 public:
  // Bound variable: writes back to out
  VarExpr(T& out, const std::string& name, CompileRandKind compile_kind = CompileRandKind::Rand,
          bool* rand_mode_ptr = nullptr, RandcCycle* randc_cycle = nullptr) :
      VarExprBase{name, compile_kind, rand_mode_ptr, randc_cycle},
      out_{&out},
      symbolic_{false} {}

  // Symbolic variable: used by for_each_i, not bound to real output
  explicit VarExpr(const std::string& name) :
      VarExprBase{name, CompileRandKind::Rand, nullptr, nullptr},
      out_{&dummy_},
      symbolic_{true} {}

  T& out() {
    return *out_;
  }
  const T& out() const {
    return *out_;
  }

  void write_back(int64_t value) override {
    *out_ = static_cast<T>(value);
  }

  int64_t read_as_int64() const override {
    return static_cast<int64_t>(*out_);
  }

  bool is_symbolic() const override {
    return symbolic_;
  }

 private:
  T* out_;
  bool symbolic_;
  T dummy_{};
};

/// 枚举随机变量：在 Z3 中用 `std::underlying_type_t<E>` 表示。
template <typename E>
  requires std::is_enum_v<E>
class VarExprEnum : public VarExprBase {
 public:
  VarExprEnum(E& out, std::string name, CompileRandKind compile_kind = CompileRandKind::Rand,
              bool* rand_mode_ptr = nullptr, RandcCycle* randc_cycle = nullptr) :
      VarExprBase{std::move(name), compile_kind, rand_mode_ptr, randc_cycle},
      out_{&out},
      symbolic_{false} {}

  explicit VarExprEnum(const std::string& name) :
      VarExprBase{std::move(name), CompileRandKind::Rand, nullptr, nullptr},
      out_{&dummy_},
      symbolic_{true} {}

  E& out() {
    return *out_;
  }
  const E& out() const {
    return *out_;
  }

  void write_back(int64_t value) override {
    using U = std::underlying_type_t<E>;
    *out_ = static_cast<E>(static_cast<U>(value));
  }

  int64_t read_as_int64() const override {
    return static_cast<int64_t>(static_cast<std::underlying_type_t<E>>(*out_));
  }

  bool is_symbolic() const override {
    return symbolic_;
  }

 private:
  E* out_;
  bool symbolic_{false};
  E dummy_{};
};

}  // namespace fuzzy

#endif  // FUZZY_AST_RAND_VAR_H
