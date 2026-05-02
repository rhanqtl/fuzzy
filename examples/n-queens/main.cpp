#include <iostream>

#include "NQueens.h"

int main() {
  auto result = fuzzy::randomize<NQueens>();
  if (result) {
    std::cout << *result << std::endl;
  } else {
    std::cerr << "Randomization failed\n";
    return 1;
  }
}
