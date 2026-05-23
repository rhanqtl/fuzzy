# fuzzy

fuzzy 是一个用 C++ 描述 **SystemVerilog 风格约束随机** 模型的库，底层使用 Z3 做可满足性求解。适合熟悉 IEEE 1800 constrained random 的验证工程师，在 C++ 测试平台或工具链中复用同类约束写法。

## 快速构建

```sh
cmake -S . -B build -DFUZZY_INCLUDE_TESTS=ON -DFUZZY_INCLUDE_EXAMPLES=ON
cmake --build build
ctest --test-dir build
```

## 示例：N 皇后

[`examples/n-queens`](examples/n-queens) 用 `FUZZY_META`、多个命名约束块、`for_each_i`、`unique` 和 `implies` 随机生成一个合法棋盘：

```cpp
struct NQueens {
  int n;
  std::vector<std::size_t> board;

  FUZZY_META(NQueens, void, FUZZY_RAND(n), FUZZY_RAND(board))
    BLOCK(board_size) {
      require(4 <= n && n <= 10);
      require(board.size() == static_cast<std::size_t>(n));
    }
    BLOCK(valid_pos) {
      for_each_i(board, [&](auto&, auto& bi) {
        require(0 <= bi && bi < n);
      });
    }
    BLOCK(solution) {
      unique(board);
      for_each_i(board, [&](auto& i, auto& bi) {
        for_each_i(board, [&](auto& j, auto& bj) {
          require(implies(i < j, bi != bj && abs(bi - bj) != j - i));
        });
      });
    }
  FUZZY_END
};
```

构建并运行：

```sh
cmake --build build --target ex_nqueens
./build/examples/ex_nqueens
```

成功时会打印 `n×n` 棋盘（`.` 为空格，`Q` 为皇后）。

## 其它示例

| 目录 | 说明 |
|------|------|
| [`examples/basic`](examples/basic) | 标量算术约束 |
| [`examples/soft`](examples/soft) | 软约束 |
| [`examples/unique`](examples/unique) | `unique` 约束 |

## 文档

| 文档 | 内容 |
|------|------|
| [`docs/UserGuide.md`](docs/UserGuide.md) | 用户手册：已支持特性（含 SystemVerilog 对照与代码示例）、未支持特性列表 |
| [`docs/semantic_differences.md`](docs/semantic_differences.md) | 与 SystemVerilog 的语义差异说明 |
| [`docs/tasks.md`](docs/tasks.md) | 开发期实现对照清单（维护用） |

## 要求

- C++20
- CMake 3.20+
- Z3（由项目 CMake 模块查找）
