# fuzzy 用户指南

fuzzy 是一个用 C++ 描述约束随机模型的库，适合熟悉 SystemVerilog（IEEE 1800）constrained random 的用户。本文按 **SystemVerilog 特性** 组织：每项给出 SV 语法示例、fuzzy 对应写法，并标注支持状态。

- 开发期实现对照：[`tasks.md`](tasks.md)
- 与 SV 的语义差异：[`semantic_differences.md`](semantic_differences.md)

## 特性速查表

| SystemVerilog 特性 | 状态 | 章节 |
|--------------------|------|------|
| `rand` | 支持 | [rand](#rand--支持) |
| `randc` | 部分支持 | [randc](#randc--部分支持) |
| 非随机 state 字段 | 支持 | [state 字段](#state-字段--支持) |
| `constraint` 命名块 | 支持 | [constraint 块](#constraint-命名块--支持) |
| `constraint_mode()` | 支持 | [constraint_mode](#constraint_mode--支持) |
| 继承 / 同名块覆盖 | 支持 | [继承](#继承与同名-constraint-块--支持) |
| `randomize()` | 支持 | [randomize](#randomize--支持) |
| `randomize() with {}` | 支持 | [randomize with](#randomize-with--支持) |
| `randomize(var_list)` | 部分支持 | [变量列表随机化](#randomizevar_list--部分支持) |
| `randomize(null)` | 支持 | [validate / check](#randomizenull--支持) |
| `std::randomize with {}` | 部分支持 | [randomize_scope](#stdrandomize-with--部分支持) |
| `pre_randomize` / `post_randomize` | 支持 | [随机化钩子](#pre_randomize--post_randomize--支持) |
| `get_randstate` / `set_randstate` | 部分支持 | [随机状态](#get_randstate--set_randstate--部分支持) |
| `rand_mode()` | 支持 | [rand_mode](#rand_mode--支持) |
| 硬约束（关系/逻辑/算术） | 支持 | [require](#硬约束-require--支持) |
| `->` 蕴含 | 支持 | [implies](#蕴含---支持) |
| `if` / `else` 约束 | 支持 | [条件约束](#if--else-约束--支持) |
| `inside` | 支持 | [inside](#inside--支持) |
| `unique` | 支持 | [unique](#unique--支持) |
| `soft` | 部分支持 | [soft](#soft--部分支持) |
| `disable soft` | 部分支持 | [disable soft](#disable-soft--部分支持) |
| `dist` | 部分支持 | [dist](#dist--部分支持) |
| `solve ... before` | 部分支持 | [solve_before](#solve-before--部分支持) |
| `foreach` | 支持 | [for_each_i](#foreach--支持) |
| 动态数组 `size` | 支持 | [动态数组 size](#动态数组-size--支持) |
| 数组归约 + `with` | 支持 | [数组归约](#数组归约--支持) |
| 关联数组 / `Dict` | 支持 | [映射容器](#映射容器--支持) |
| `enum` | 支持 | [枚举](#enum--支持) |
| 位选 / 拼接 / `==?` | 部分支持 | [位运算 helper](#位运算-helper--部分支持) |
| 约束内函数 | 部分支持 | [约束内函数](#约束内函数调用--部分支持) |
| `local::` | 替代方案 | [local 作用域](#local--替代方案) |
| `randcase` / `randsequence` / `$urandom` 等 | 未支持 | [未支持特性](#未支持特性) |
| `real` / `string` 约束 | 未支持 | [未支持特性](#未支持特性) |
| 完整 bit-vector / 4-state | 未支持 | [未支持特性](#未支持特性) |

---

## 构建与测试

```sh
cmake -S . -B build -DFUZZY_INCLUDE_TESTS=ON -DFUZZY_INCLUDE_EXAMPLES=ON
cmake --build build
ctest --test-dir build
```

覆盖率（可选）：

```sh
cmake -S . -B build-coverage -DFUZZY_INCLUDE_TESTS=ON -DFUZZY_COVERAGE=ON
cmake --build build-coverage --target fuzzy-lib-coverage-check
```

综合示例（N 皇后）：[`examples/n-queens`](../examples/n-queens)。

---

## 已支持特性

### `rand` — 支持

**SystemVerilog**

```systemverilog
class Packet;
  rand int kind;
  rand int len;
  constraint valid {
    kind inside {0, 1, 2};
    len inside {[1:128]};
  }
endclass
```

**fuzzy**

```cpp
struct Packet {
  int kind{0};
  int len{0};

  FUZZY_META(Packet, void, FUZZY_RAND(kind), FUZZY_RAND(len))
  BLOCK(valid) {
    inside(kind, 0, 1, 2);
    require(len >= 1);
    require(len <= 128);
  }
  FUZZY_END
};
```

**说明**：`FUZZY_RAND(x)` 标记字段参与随机化；未标记字段为 state，按当前值参与约束。

---

### `randc` — 部分支持

**SystemVerilog**

```systemverilog
class S;
  randc bit [1:0] y;  // 0..3 无放回循环
  constraint c { y inside {[0:3]}; }
endclass
```

**fuzzy**

```cpp
struct S {
  int y{};
  FUZZY_META(S, void, FUZZY_RANDC(y))
  BLOCK(c) {
    require(y >= 0);
    require(y <= 3);
  }
  FUZZY_END
};
```

**说明**：仅支持 **标量** `FUZZY_RANDC` 字段；与硬约束冲突导致域耗尽时会清空周期重试。详见 [`semantic_differences.md`](semantic_differences.md#randc)。

---

### state 字段 — 支持

**SystemVerilog**

```systemverilog
class A;
  int mode;      // 非 rand，固定参与约束
  rand int x;
  constraint c {
    if (mode == 1) x == 42;
    else x == 7;
  }
endclass
```

**fuzzy**

```cpp
struct A {
  int mode{0};
  int x{0};
  FUZZY_META(A, void, mode, FUZZY_RAND(x))  // mode 无 FUZZY_RAND
  BLOCK(b) {
    constraint_if(mode == 1, [&] { require(x == 42); });
    constraint_if(mode == 0, [&] { require(x == 7); });
  }
  FUZZY_END
};
```

---

### constraint 命名块 — 支持

**SystemVerilog**

```systemverilog
constraint board_size { n inside {[4:10]}; board.size() == n; }
constraint solution { unique {board}; /* ... */ }
```

**fuzzy**

```cpp
FUZZY_META(NQueens, void, FUZZY_RAND(n), FUZZY_RAND(board))
  BLOCK(board_size) {
    require(4 <= n && n <= 10);
    require(board.size() == static_cast<std::size_t>(n));
  }
  BLOCK(solution) {
    unique(board);
    // ...
  }
FUZZY_END
```

**说明**：`BLOCK(name)` 对应 SV 命名 `constraint` 块，可用 `constraint_mode` 开关。

---

### `constraint_mode()` — 支持

**SystemVerilog**

```systemverilog
pkt.constraint_mode("valid", 0);  // 禁用
pkt.constraint_mode("valid", 1);  // 启用
```

**fuzzy**

```cpp
pkt.fuzzy_constraint_mode("valid", 0);
pkt.fuzzy_constraint_mode("valid", 1);
```

**说明**：禁用同名块时，派生类与基类的**同名**块一并跳过，不回退基类版本。见 [`semantic_differences.md`](semantic_differences.md#继承中的同名-constraint-block)。

---

### 继承与同名 constraint 块 — 支持

**SystemVerilog**

```systemverilog
class Base;
  rand int x;
  constraint base_c { x inside {[0:10]}; }
endclass
class Derived extends Base;
  rand int y;
  constraint derived_c { x > 5; y == x + 1; }
endclass
```

**fuzzy**

```cpp
struct Base {
  int x{0};
  FUZZY_META(Base, void, FUZZY_RAND(x))
  BLOCK(base_c) { require(x >= 0); require(x <= 10); }
  FUZZY_END
};

struct Derived : Base {
  int y{0};
  FUZZY_META(Derived, (Base), FUZZY_RAND(y))
  BLOCK(derived_c) {
    require(x > 5);
    require(y == x + 1);
  }
  FUZZY_END
};
```

**说明**：`FUZZY_META(Derived, (Base), ...)` 合并基类约束；派生类**同名** `BLOCK` 覆盖基类同名块。

---

### `randomize()` — 支持

**SystemVerilog**

```systemverilog
Packet pkt = new();
if (!pkt.randomize()) $error("failed");
```

**fuzzy**

```cpp
// 工厂式，返回 std::optional
auto pkt = fuzzy::randomize<Packet>();
if (!pkt) { /* 不可满足 */ }

// 已有对象
Packet pkt{};
bool ok = fuzzy::randomize(pkt);

// 显式种子
auto pkt2 = fuzzy::randomize<Packet>(1234u);

// 复用 Model（randc 周期、连续随机化）
fuzzy::Model<Packet> model{1234u};
bool ok1 = model.randomize(pkt);
bool ok2 = model.randomize(pkt);
```

---

### `randomize() with {}` — 支持

**SystemVerilog**

```systemverilog
if (!pkt.randomize() with { kind == 1; len >= 32; })
  $error("failed");
```

**fuzzy**

```cpp
auto pkt = fuzzy::randomize_with<Packet>(
    [](auto& p, fuzzy::dsl::DslContext& ctx) {
      ctx.require(p.kind == 1);
      ctx.require(p.len >= 32);
    });

Packet pkt{};
bool ok = fuzzy::randomize_with(pkt, [](auto& p, fuzzy::dsl::DslContext& ctx) {
  ctx.require(p.kind == 2);
});
```

**说明**：临时约束仅影响当前调用，不修改 `BLOCK` 定义。

---

### `randomize(var_list)` — 部分支持

**SystemVerilog**

```systemverilog
pkt.randomize() with { len dist { [32:64] := 1 }; };  // 只随机化 len
```

**fuzzy**

```cpp
Packet pkt{.kind = 1, .len = 0};
bool ok = fuzzy::randomize(pkt, {"len"});

bool ok2 = fuzzy::randomize_with(pkt, {"len"},
    [](auto& p, fuzzy::dsl::DslContext& ctx) {
      ctx.require(p.len >= 32);
    });
```

**说明**：按 **字段名字符串** pin 未列字段；不等价于 SV 句柄、进程级随机稳定性等完整语义。见 [`semantic_differences.md`](semantic_differences.md#随机化入口)。

---

### `randomize(null)` — 支持

**SystemVerilog**

```systemverilog
if (!pkt.randomize(null))  // 不随机化，只检查当前值
  $error("inconsistent");
```

**fuzzy**

```cpp
Packet pkt{.kind = 0, .len = 8};
bool ok = fuzzy::validate(pkt);
bool also_ok = fuzzy::check(pkt);

// 或：空字段列表只 pin、不改动 rand 字段
bool ok2 = fuzzy::randomize(pkt, {});

// N 皇后示例中验证解
if (result && !fuzzy::validate(*result)) { /* invalid */ }
```

---

### `std::randomize() with {}` — 部分支持

**SystemVerilog**

```systemverilog
int x, y;
std::randomize(x, y) with { x + y == 10; x >= 0; y >= 0; };
```

**fuzzy**

```cpp
int x = 0, y = 0;
bool ok = fuzzy::randomize_scope(x, y).with(
    1234u,
    [](fuzzy::dsl::DslContext& ctx, auto& rx, auto& ry) {
      ctx.require(rx >= 0);
      ctx.require(ry >= 0);
      ctx.require(rx + ry == 10);
    });
```

**说明**：C++ 局部标量工程子集，非 SV 调度与句柄语义。

---

### `get_randstate` / `set_randstate` — 部分支持

**SystemVerilog**

```systemverilog
string state = obj.get_randstate();
obj.set_randstate(state);
```

**fuzzy**

```cpp
fuzzy::Model<Packet> model{1234u};
auto state = model.rand_state();
model.randomize(pkt);
model.set_rand_state(state);
model.randomize(pkt);  // 复现与上一次 set 后相同的“下一次”序列
```

**说明**：保存模型 seed 与调用计数，非 SV 进程级随机稳定性完整等价。

---

### `pre_randomize` / `post_randomize` — 支持

**SystemVerilog**

```systemverilog
function void pre_randomize();
  lo = 40;
endfunction
constraint c { x >= lo; }
```

**fuzzy**

```cpp
struct A {
  int lo{5};
  int x{0};
  void fuzzy_pre_randomize() { lo = 40; }
  void fuzzy_post_randomize() { /* 仅成功时调用 */ }
  FUZZY_META(A, void, FUZZY_RAND(x), lo)
  BLOCK(b) { require(x >= lo); require(x <= 100); }
  FUZZY_END
};
```

---

### `rand_mode()` — 支持

**SystemVerilog**

```systemverilog
pkt.len.rand_mode(0);  // 保持当前值
```

**fuzzy**

```cpp
FUZZY_RAND_MODE(pkt, len) = false;
fuzzy::randomize(pkt);
FUZZY_RAND_MODE(pkt, len) = true;

// RandMember 字段：随机性在成员上
fuzzy::RandMember<int> a{};
FUZZY_META(S, void, FUZZY_RANDM(a))
// s.a.rand_mode(0);
```

---

### 硬约束 `require` — 支持

**SystemVerilog**

```systemverilog
constraint c {
  x >= 0; x <= 100;
  x != y;
  (x == 0) || (y == 1);
}
```

**fuzzy**

```cpp
require(x >= 0);
require(x <= 100);
require(x != y);
require((x == 0) || (y == 1));
```

**说明**：向 `require` 传入 C++ `bool` 时，`true` 为 no-op，`false` 生成不可满足约束。

---

### 蕴含 `->` — 支持

**SystemVerilog**

```systemverilog
constraint c {
  foreach (board[i]) foreach (board[j])
    (i < j) -> (board[i] != board[j] &&
                abs(board[i]-board[j]) != j-i);
}
```

**fuzzy**

```cpp
for_each_i(board, [&](auto& i, auto& bi) {
  for_each_i(board, [&](auto& j, auto& bj) {
    require(implies(i < j, bi != bj && abs(bi - bj) != j - i));
  });
});
```

---

### `if` / `else` 约束 — 支持

**SystemVerilog**

```systemverilog
constraint c {
  if (t == 0) x == 100;
  else x == 200;
}
```

**fuzzy**

```cpp
constraint_if(mode == 0, [&] { require(len <= 16); });

constraint_if_else(
    t == 0,
    [&] { require(x == 100); },
    [&] { require(x == 200); });
```

---

### `inside` — 支持

**SystemVerilog**

```systemverilog
constraint c { x inside {1, 3, [10:12]}; }
```

**fuzzy**

```cpp
inside(x, 1, 3, fuzzy::dsl::inside_span(10, 12));
```

---

### `unique` — 支持

**SystemVerilog**

```systemverilog
constraint c { unique {board}; }
// 或多标量
constraint c { unique {a, b, c}; }
```

**fuzzy**

```cpp
unique(board);
unique(a, b, c);
```

---

### `soft` — 部分支持

**SystemVerilog**

```systemverilog
constraint c {
  soft len == 64;
  len inside {[1:128]};
}
```

**fuzzy**

```cpp
soft(len == 64);
require(len >= 1);
require(len <= 128);
```

**说明**：与硬约束冲突时 fuzzy 用启发式丢弃软约束，**不保证**与仿真器丢弃顺序一致。见 [`semantic_differences.md`](semantic_differences.md#soft)。

---

### `disable soft` — 部分支持

**SystemVerilog**

```systemverilog
pkt.randomize() with { disable soft len == 64; };
```

**fuzzy**

```cpp
fuzzy::randomize_with(pkt, [](auto& p, fuzzy::dsl::DslContext& ctx) {
  ctx.disable_soft(p.len == 64);
});
```

**说明**：按表达式结构匹配；不覆盖 SV 全部匹配规则。

---

### `dist` — 部分支持

**SystemVerilog**

```systemverilog
constraint c {
  kind dist { 0 := 1, 1 := 3, 2 := 1 };
  // 或 range: [10:20] :/ 5
}
```

**fuzzy**

```cpp
dist(kind, {{0, 1}, {1, 3}, {2, 1}});
require(kind == 0 || kind == 1 || kind == 2);
```

**说明**：显式整数 `(值, 权重)` 列表；先限制可行域再加权选一个值。不支持 `:=` / `:/` 的 range 分摊语义与完整联合分布。见 [`semantic_differences.md`](semantic_differences.md#dist)。

---

### `solve ... before` — 部分支持

**SystemVerilog**

```systemverilog
constraint c {
  solve kind before len;
}
```

**fuzzy**

```cpp
solve_before(kind, len);
```

**说明**：排序提示，不改变可行解集；`randc` 变量出现在 `solve_before` 中会拒绝求解。

---

### `foreach` — 支持

**SystemVerilog**

```systemverilog
constraint valid_pos {
  foreach (board[i])
    board[i] inside {[0:n-1]};
}
```

**fuzzy**

```cpp
for_each_i(board, [&](auto& i, auto& bi) {
  require(0 <= bi && bi < n);
});
```

**说明**：`for_each_kv` 用于 `fuzzy::Dict` / `std::map` 等映射。多维 `foreach` **未支持**。

---

### 动态数组 size — 支持

**SystemVerilog**

```systemverilog
constraint c { board.size() == n; }
```

**fuzzy**

```cpp
require(board.size() == static_cast<std::size_t>(n));
```

---

### 数组归约 — 支持

**SystemVerilog**

```systemverilog
constraint c {
  xs.size() == 3;
  xs.sum() == 12;
  xs.sum() with (item * 2) == 24;  // SV 语法
}
```

**fuzzy**

```cpp
require(xs.size() == 3);
require(array_sum(xs) == 12);
require(array_sum(xs, [](auto&, auto& e) -> fuzzy::Expr& { return e * 2; }) == 24);
require(array_product(xs) == 24);
require(array_min(xs) >= 0);
require(array_max(xs) <= 10);
// array_and / array_or / array_xor：按元素非零为 true，结果为 0/1
```

---

### 映射容器 — 支持

**SystemVerilog**

```systemverilog
// 关联数组 rand 键值
constraint c {
  m.size() == 3;
  foreach (m[key])
    m[key] >= key;
}
```

**fuzzy**

```cpp
#include "fuzzy/Dict.h"

fuzzy::Dict<Color, int> m;  // 或 std::map<Color, int>
FUZZY_META(S, void, FUZZY_RAND(m))
BLOCK(c) {
  require(m.size() == 3);
  for_each_kv(m, [&](auto&, auto& k, auto& v) {
    require(v >= k);
  });
}
FUZZY_END
```

---

### `enum` — 支持

**SystemVerilog**

```systemverilog
typedef enum {A, B, C} kind_e;
rand kind_e kind;
constraint c { kind inside {A, B, C}; }
```

**fuzzy**

```cpp
enum class Kind { A = 0, B = 1, C = 2 };
Kind kind{Kind::A};
FUZZY_META(Item, void, FUZZY_RAND(kind))
BLOCK(valid) { inside(kind, Kind::A, Kind::B, Kind::C); }
FUZZY_END
```

---

### 位运算 helper — 部分支持

**SystemVerilog**

```systemverilog
constraint c {
  x[2] == 1;
  x[3:1] == 5;
  {hi, lo} == joined;
  x ==? 4'b1010;  // care mask
}
```

**fuzzy**

```cpp
require(bit_select(x, 2) == 1);
require(bit_slice(x, 3, 1) == 5);
require(joined == bit_concat(hi, 2, lo));
require(wildcard_eq(x, 0b1010, 0b1110));
require(wildcard_ne(x, 0b0010, 0b1110));
```

**说明**：非负整数的 2-state 编码子集，非完整 bit-vector / 4-state。见 [`semantic_differences.md`](semantic_differences.md#位运算-helper)。

---

### 约束内函数调用 — 部分支持

**SystemVerilog**

```systemverilog
function int foo(int v); return v + 1; endfunction
constraint c { x + y == foo(z); }
```

**fuzzy**

```cpp
struct Equation {
  int foo(int v) const { return v + 1; }
  FUZZY_META(Equation, void, FUZZY_RAND(x), FUZZY_RAND(y), FUZZY_RAND(z))
  auto Foo = FUZZY_DECLARE_FUNC(&Equation::foo);
  BLOCK(c) { require(x + y == invoke(__obj, Foo, z)); }
  FUZZY_END
};
```

**说明**：受限函数调用；求值方向与 SV 不保证一致。

---

### `local::` — 替代方案

**SystemVerilog**

```systemverilog
int cap = 10;
obj.randomize() with { local::cap == x; };
```

**fuzzy**

```cpp
int cap = 10;
fuzzy::randomize_with(obj, [&](auto& o, fuzzy::dsl::DslContext& ctx) {
  ctx.require(cap == o.x);  // lambda 捕获调用点局部变量
});
```

**说明**：不实现 `local::` 关键字；用 C++ lambda 捕获达到相同效果。

---

## 综合示例：N 皇后

完整代码见 [`examples/n-queens/NQueens.h`](../examples/n-queens/NQueens.h)。

**SystemVerilog（示意）**

```systemverilog
class NQueens;
  rand int n;
  rand int board[];
  constraint board_size { n inside {[4:10]}; board.size() == n; }
  constraint valid_pos { foreach (board[i]) board[i] inside {[0:n-1]}; }
  constraint solution {
    unique {board};
    foreach (board[i]) foreach (board[j])
      (i < j) -> (board[i]!=board[j] && abs(board[i]-board[j])!=j-i);
  }
endclass
```

运行：`cmake --build build --target ex_nqueens && ./build/examples/ex_nqueens`

---

## 未支持特性

以下 SystemVerilog 能力在 fuzzy 中**尚无**对应 `FUZZY_META` 路径或公开 API。每项给出 SV 示例与 fuzzy 现状。

### 过程式随机（非 constraint 语法）

**SystemVerilog**

```systemverilog
randcase (1: a=1;, 3: a=2;)
randsequence (main: { A B; })
x = $urandom_range(0, 100);
```

**fuzzy**：不支持。请用 C++ 标准库或应用层逻辑。

---

### `static` / `extern` / `pure constraint`

**SystemVerilog**

```systemverilog
static constraint C { ... }
extern constraint C;
pure constraint C;
```

**fuzzy**：仅支持实例 `BLOCK`；无 static/extern/pure 约束块。

---

### `super.randomize()`

**SystemVerilog**

```systemverilog
if (!super.randomize()) return 0;
```

**fuzzy**：继承通过 `FUZZY_META(Derived, (Base), ...)` 合并约束，无过程式 `super.randomize()`。

---

### 多维 `foreach`

**SystemVerilog**

```systemverilog
foreach (m[i,j]) m[i][j] > 0;
```

**fuzzy**：仅一维 `for_each_i` / `for_each_kv`；多维需手写嵌套或展开。

---

### `real` 约束

**SystemVerilog**

```systemverilog
rand real r;
constraint c { r inside {[0.0:1.0]}; }
```

**fuzzy**：未实现实数随机变量与约束。

---

### `string` 作为 `FUZZY_RAND` 字段

**SystemVerilog**

```systemverilog
rand string s;
constraint c { s.len() inside {[1:8]}; }
```

**fuzzy**：内部有 `RandString` 类型，但未在 `FUZZY_META` 宏路径中提供稳定的字符串随机化与约束 API。

---

### 完整 bit-vector / 4-state / 位宽传播

**SystemVerilog**

```systemverilog
rand logic signed [7:0] v;
constraint c { v + 1 inside {[0:255]}; }  // 溢出语义依 LRM
```

**fuzzy**：仅有整数编码的 `bit_select` / `bit_slice` / `bit_concat` / `wildcard_*` helper，无符号扩展、4-state、溢出截断。

---

### 完整 `dist` range 语义（`:=` / `:/`）

**SystemVerilog**

```systemverilog
x dist { [10:20] :/ 5, 25 := 1 };
```

**fuzzy**：仅支持离散 `(值, 权重)` 对；不支持 range 权重分摊。

---

### 对象句柄、嵌套对象、对象数组

**SystemVerilog**

```systemverilog
rand Packet pkt;
rand Packet queue[$];
constraint c { pkt.len < 100; foreach (queue[i]) queue[i].valid; }
```

**fuzzy**：`std::vector<int>` 等整型数组支持良好；用户自定义类型对象数组不会通过 `FUZZY_META` 自动展开为子对象约束（测试中有手写 `RandVar` 低级 API，非推荐用户路径）。

---

### 数组 `find`、整体相等 `arr1 == arr2`

**SystemVerilog**

```systemverilog
constraint c { arr1 == arr2; xs.find(x) with (x > 0) != -1; }
```

**fuzzy**：未实现；可用 `for_each_i` 表达元素级关系。

---

### 进程级随机稳定性

**SystemVerilog**

```systemverilog
// 线程/进程 RNG 与对象随机稳定性规则（LRM 第 18 章相关）
```

**fuzzy**：`Model::rand_state()` 仅保存单模型 seed 与调用计数，不等价于 SV 完整稳定性规则。

---

## 语义差异索引

以下特性已实现但与 SystemVerilog **不完全等价**，详细说明见 [`semantic_differences.md`](semantic_differences.md)：

| 主题 | 摘要 |
|------|------|
| `soft` | 丢弃顺序为工程启发式 |
| `dist` | 离散加权子集，非 `:=`/`:/` range |
| `solve_before` | 提示不改变解集；与仿真器排序可能不同 |
| `randc` | 标量周期；多变量耗尽时清空重试 |
| `randomize(var_list)` | 字段名 pin，非完整 SV 列表语义 |
| 随机状态 | 模型级 seed/counter，非进程级 |
| 位 helper | 整数 2-state 子集 |
| 约束内函数 | 受限调用，求值方向可能不同 |
| `disable soft` | 表达式结构匹配子集 |
| 继承同名块 | `constraint_mode` 禁用时不回退基类 |

依赖上述语义时，请针对场景补充测试并阅读差异文档。
