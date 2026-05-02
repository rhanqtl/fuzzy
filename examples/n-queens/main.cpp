#include <iostream>

#include "NQueens.h"

int main() {
  auto result = fuzzy::randomize<NQueens>();
  if (result) {
    if (!fuzzy::validate(*result)) {
      std::cout << "ERROR: invalid solution\n";
    } else {
      std::cout << *result << std::endl;
    }
  } else {
    std::cout << "ERROR: randomization failed\n";
    return 1;
  }
}
