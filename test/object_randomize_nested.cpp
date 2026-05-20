#include <cstdint>
#include <format>
#include <functional>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string_view>
#include <vector>

#include "fuzzy/fuzzy.h"

class Address {
  friend class fuzzy::Model<Address>;

 public:
  enum class CountryCode {
    INVALID,
    EN_US,
    ZH_CN,
  };

 public:
  Address(CountryCode country, std::string_view city, std::string_view details) :
      country_{country},
      city_{city},
      details_{details} {}

  Address() :
      Address(CountryCode::INVALID, "", "") {}

 public:
  CountryCode country() const {
    return country_;
  }
  std::string_view city() const {
    return city_;
  }
  std::string_view details() const {
    return details_;
  }

 private:
  CountryCode country_;
  std::string city_;
  std::string details_;
};

std::ostream& operator<<(std::ostream& os, const Address::CountryCode c) {
  switch (c) {
    case Address::CountryCode::INVALID:
      return os << "<unk>";
    case Address::CountryCode::EN_US:
      return os << "en_US";
    case Address::CountryCode::ZH_CN:
      return os << "zh_CN";
  }
}

std::ostream& operator<<(std::ostream& os, const Address& addr) {
  std::ostringstream oss;
  oss << addr.country();
  return os << std::format("Address(country={}, city=\"{}\", details=\"{}\")", oss.str(),
                           addr.city(), addr.details());
}

class Person {
  friend class fuzzy::Model<Person>;

 public:
  Person() :
      Person{"", 0} {}
  Person(std::string_view name, uint32_t age, const Address* addrp = nullptr) :
      name_{name},
      age_{age} {
    if (addrp)
      addr_ = Address(*addrp);
  }

 public:
  std::string_view name() const {
    return name_;
  }
  uint32_t age() const {
    return age_;
  }
  const Address& addr() const {
    return addr_;
  }

 private:
  std::string name_;
  unsigned age_;
  Address addr_;
};

std::ostream& operator<<(std::ostream& os, const Person& p) {
  return os << std::format("Person(name=\"{}\", age={})", p.name(), p.age());
}

// Data members in a model shall mirror randomizable data members in the
// target class.
namespace fuzzy {
template <>
struct Model<Address> : ModelBase {
  RandVar<Address::CountryCode>& country_;
  RandVar<std::string>& city_;
  RandVar<std::string>& details_;

  Model(Address& addr) :
      country_{get_var(addr.country_)},
      city_{get_var(addr.city_)},
      details_{get_var(addr.details_)} {}
};

template <>
struct Model<Person> : ModelBase {
  RandVar<std::string>& name;
  RandVar<unsigned>& age;
  Model<Address> addr;

  fuzzy::ConstraintBlock short_name;
  fuzzy::ConstraintBlock long_name;
  fuzzy::ConstraintBlock range_of_age;

  Model(Person& p) :
      name{get_var(p.name_)},
      age{get_var(p.age_)},
      addr{p.addr_},
      short_name{[this] { _short_name_thunk(); }},
      long_name{[this] { _long_name_thunk(); }, false},
      range_of_age{[this] { _normal_age_thunk(); }} {}

 private:
  void _short_name_thunk() {
    0 < name.length() && name.length() <= 10;
  }
  void _long_name_thunk() {
    0 < name.length() && name.length() <= 255;
  }
  void _normal_age_thunk() {
    0 < age&& age <= 100;
  }
};
}  // namespace fuzzy
#if 0
class PersonList {
 public:
  template <typename IterT>
  PersonList(IterT first, IterT last) : data_{first, last} {}

 private:
  std::vector<Person> data_;
};

namespace fuzzy {
template <>
struct Model<PersonList> {
  static PersonList randomize() {
    fuzzy::usize_var size;
    size.inside(1, 10);
    for (int i = 0; i < 10; i++) {
      if (i == 0) {
        data[i].age > 0;
      } else {
        data[i - 1].age < data[i].age;
      }
    }
    return PersonList{vec.begin(), vec.end()};
  }
};
}  // namespace fuzzy
#endif

#if 0
template <template <typename...> typename RandomAccessible>
struct F<RandomAccessible<Person>> {
  static RandomAccessible<Person> randomize() {
    RandomAccessible<Person> vec;
    for () return vec;
  }
};
}  // namespace fuzzy
#endif

int main() {
  Person p;
  const auto ok = fuzzy::randomize(p);
  if (!ok)
    std::cerr << "Randomization failed\n";
  else
    std::cout << p << '\n';
}
