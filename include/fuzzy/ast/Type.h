#pragma once

#include <cstddef>

namespace fuzzy {
enum class TypeKind {
  BitVec,
  Vector,
  Array,
};

class Type {
 public:
  explicit Type(TypeKind kind) :
      kind_{kind} {}

  virtual ~Type() = default;

 public:
  TypeKind kind() const {
    return kind_;
  }

 private:
  TypeKind kind_;
};

// Per cppreference of `std::is_integral`:
//   ... if T is the type bool, char, char8_t(since C++20), char16_t, char32_t,
//   wchar_t, short, int, long, long long, or any implementation-defined extended
//   integer types, including any signed, unsigned, and cv-qualified variants ...
// The number of types is small, so we enumerate them.
class BitVecType : public Type {
 public:
  BitVecType(std::size_t width, bool is_signed) :
      Type{TypeKind::BitVec},
      width_{width},
      signed_{is_signed} {}

 public:
  auto width() const {
    return width_;
  }
  auto is_signed() const {
    return signed_;
  }

 private:
  std::size_t width_;
  bool signed_;
};

class VectorType : public Type {
 public:
  VectorType(Type* elem_type) :
      Type{TypeKind::Vector},
      elem_type_{elem_type} {}

  auto* element_type() const {
    return elem_type_;
  }

 private:
  Type* elem_type_;
};

class ArrayType : public Type {
 public:
  ArrayType(Type* elem_type, std::size_t n) :
      Type{TypeKind::Vector},
      elem_type_{elem_type},
      n_{n} {}

  auto* element_type() const {
    return elem_type_;
  }
  auto num_elements() const {
    return n_;
  }

 private:
  Type* elem_type_;
  std::size_t n_;
};
}  // namespace fuzzy
