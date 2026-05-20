#include <cstdlib>
#include <format>
#include <iostream>
#include <ostream>

#include "fuzzy/fuzzy.h"

class Person {
  friend class fuzzy::Model<Person>;

  std::string name;
  int age;

  auto to_string() const -> std::string {
    return std::format("Person(name=\"{}\", age={})", name, age);
  }
  friend std::ostream& operator<<(std::ostream& out, const Person& p) {
    return out << p.to_string();
  }
};

namespace fuzzy {
template <>
struct Model<Person> : ModelBase {
  friend ModelBase;

  RandVar<std::string>& name;
  RandVar<int>& age;

  FUZZY_CONSTRAINT_BLOCK(length_of_name) {
    0 < name.length() && name.length() < 128;
  }
  FUZZY_CONSTRAINT_BLOCK(range_of_age) {
    0 <= age;
    age <= 100;
  };

  Model(ExprContext& ctx, Person& p) :
      ref_{p},
      name{ctx.create_var(p.name)},
      age{ctx.create_var(p.age)} {}

 private:
  void collect_constraints() {
    if (length_of_name.enabled())
      length_of_name();
    if (range_of_age.enabled())
      range_of_age();
  }

 private:
  Person& ref_;
};
}  // namespace fuzzy

int main() {
  Person p;
  const auto ok = fuzzy::randomize(p);
  if (!ok)
    std::cerr << "Randomization failed\n";
  else
    std::cout << p << '\n';
}
