#include <iostream>

#include "fuzzy/fuzzy.h"

#include "equation.h"

int main() {
  auto res = fuzzy::randomize<Equation>();
  if (res)
    std ::cout << *res << '\n';
  else
    std::cerr << "Failed to randomize\n";
}
