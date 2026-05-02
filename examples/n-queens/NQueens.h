#ifndef FUZZY_EXAMPLE_N_QUEENS_H
#define FUZZY_EXAMPLE_N_QUEENS_H

#include <cstddef>
#include <iostream>
#include <vector>

#include "fuzzy/fuzzy.h"

struct NQueens {
  int n;
  std::vector<std::size_t> board;

  FUZZY_META(NQueens, n, board)
    BLOCK(board_size) {
      constrain(4 <= n && n <= 10);
      constrain(board.size() == n);
    }
    BLOCK(valid_pos) {
      for_each_i(board, [&](auto& i, auto& bi) {
        constrain(0 <= bi && bi < n);
      });
    }
    BLOCK(solution) {
      unique(board);
      for_each_i(board, [&](auto& i, auto& bi) {
        for_each_i(board, [&](auto& j, auto& bj) {
          constrain(implies(i < j, bi != bj && abs(bi - bj) != j - i));
        });
      });
    }
  FUZZY_END

  friend std::ostream& operator<<(std::ostream& os, const NQueens& q) {
    for (int i = 0; i < q.n; i++) {
      std::string line(q.n, '.');
      line[q.board[i]] = 'Q';
      os << line << "\n";
    }
    return os;
  }
};

#endif  // FUZZY_EXAMPLE_N_QUEENS_H
