# fuzzy 用户指南

fuzzy 是一个用 C++ 描述约束随机模型的库，适合熟悉 SystemVerilog constrained random 的用户。本文只说明可用特性和推荐用法；开发期实现对照请看 [`tasks.md`](tasks.md)，语义差异请看 [`semantic_differences.md`](semantic_differences.md)。

## 构建与测试

```sh
cmake -S . -B build -DFUZZY_INCLUDE_TESTS=ON
cmake --build build
ctest --test-dir build
```

如果需要覆盖率检查：

```sh
cmake -S . -B build-coverage -DFUZZY_INCLUDE_TESTS=ON -DFUZZY_COVERAGE=ON
cmake --build build-coverage --target fuzzy-lib-coverage-check
```

## 定义随机模型

在结构体或类中使用 `FUZZY_META` 声明随机字段和普通状态字段，在 `BLOCK(name)` 中写约束。

```cpp
#include "fuzzy/fuzzy.h"

struct Packet {
  int kind{0};
  int len{0};

  FUZZY_META(Packet, void, FUZZY_RAND(kind), FUZZY_RAND(len))
  BLOCK(valid) {
    inside(kind, 0, 1, 2);
    require(len >= 1);
    require(len <= 128);
    constraint_if(kind == 0, [&] { require(len <= 16); });
  }
  FUZZY_END
};
```

- `FUZZY_RAND(x)` 表示字段参与随机化。
- `FUZZY_RANDC(x)` 表示标量字段按循环不重复方式随机化。
- 未标记为 `FUZZY_RAND` / `FUZZY_RANDC` 的字段是状态字段，随机化时按当前值参与约束。
- `BLOCK(name)` 是可命名、可开关的约束块。

## 随机化入口

工厂式随机化返回 `std::optional<T>`：

```cpp
auto pkt = fuzzy::randomize<Packet>();
if (!pkt) {
  // 约束不可满足
}
```

对已有对象随机化：

```cpp
Packet pkt{};
bool ok = fuzzy::randomize(pkt);
```

需要复现结果时传入显式种子：

```cpp
auto pkt = fuzzy::randomize<Packet>(1234u);
```

需要在同一个对象上连续随机化，或保留 `randc` 周期状态时，可以复用同一个模型对象：

```cpp
Packet pkt{};
fuzzy::Model<Packet> model{1234u};

bool ok1 = model.randomize(pkt);
bool ok2 = model.randomize(pkt);
```

需要暂停并恢复随机序列时，可以保存并恢复模型状态：

```cpp
auto state = model.rand_state();
bool ok3 = model.randomize(pkt);
model.set_rand_state(state);
bool ok4 = model.randomize(pkt);  // 使用与 ok3 相同的下一次随机状态
```

## 单次附加约束

使用 `randomize_with` 为一次随机化调用添加临时约束。

```cpp
auto pkt = fuzzy::randomize_with<Packet>(
    [](auto& p, fuzzy::dsl::DslContext& ctx) {
      ctx.require(p.kind == 1);
      ctx.require(p.len >= 32);
    });
```

对已有对象也可以添加临时约束：

```cpp
Packet pkt{};
bool ok = fuzzy::randomize_with(
    pkt,
    [](auto& p, fuzzy::dsl::DslContext& ctx) {
      ctx.require(p.kind == 2);
    });
```

临时约束只影响当前调用，不会改变模型中定义的 `BLOCK`。

## 变量列表随机化

可以只随机化指定字段，未列出的字段按当前值参与约束：

```cpp
Packet pkt{.kind = 1, .len = 0};
bool ok = fuzzy::randomize(pkt, {"len"});
```

也可以和单次附加约束组合：

```cpp
bool ok = fuzzy::randomize_with(
    pkt,
    {"len"},
    [](auto& p, fuzzy::dsl::DslContext& ctx) {
      ctx.require(p.len >= 32);
    });
```

传入空列表时，不随机化任何字段，只检查当前值是否可满足。

局部标量变量可以使用 `randomize_scope(...).with(...)`：

```cpp
int x = 0;
int y = 0;

bool ok = fuzzy::randomize_scope(x, y).with(
    1234u,
    [](fuzzy::dsl::DslContext& ctx, auto& rx, auto& ry) {
      ctx.require(rx >= 0);
      ctx.require(ry >= 0);
      ctx.require(rx + ry == 10);
    });
```

## 校验当前对象

这适合替代 SystemVerilog 中“只检查当前值是否满足约束”的场景。

使用 `validate` 检查当前字段值是否满足模型约束。

```cpp
Packet pkt{.kind = 0, .len = 8};
bool ok = fuzzy::validate(pkt);
```

也可以通过 `Model<T>` 检查已有对象：

```cpp
fuzzy::Model<Packet> model{1234u};
bool ok = model.validate(pkt);
```

## 常用约束写法

### 基本硬约束

```cpp
require(x >= 0);
require(x <= 100);
require(x != y);
require((x == 0) || (y == 1));
```

### 蕴含

```cpp
require(implies(mode == 1, len >= 64));
```

### 条件约束

```cpp
constraint_if(mode == 0, [&] {
  require(len <= 16);
});

constraint_if_else(
    mode == 1,
    [&] { require(len >= 64); },
    [&] { require(len < 64); });
```

### `inside`

```cpp
inside(x, 1, 3, fuzzy::dsl::inside_span(10, 12));
```

### `unique`

```cpp
unique(xs);
unique(a, b, c);
```

### `soft`

```cpp
soft(len == 64);
```

`soft` 是偏好约束；当它与硬约束冲突时，随机化仍可成功。

在单次附加约束中可以禁用匹配的软约束：

```cpp
bool ok = fuzzy::randomize_with(
    pkt,
    [](auto& p, fuzzy::dsl::DslContext& ctx) {
      ctx.disable_soft(p.len == 64);
    });
```

### `dist`

```cpp
dist(kind, {{0, 1}, {1, 3}, {2, 1}});
```

`dist` 可用于表达显式整数取值偏好。fuzzy 会在当前可满足的候选值中按权重选择；它和 SystemVerilog `dist` 的 `:=` / `:/` range 语义及完整联合分布模型不完全相同，需要精确分布时请单独验证统计结果。

### `solve_before`

```cpp
solve_before(kind, len);
```

`solve_before` 是求解顺序提示，不会改变可行解集合。

## 数组和映射

动态数组可以约束 size、元素和聚合值。

```cpp
struct Bag {
  std::vector<int> xs;

  FUZZY_META(Bag, void, FUZZY_RAND(xs))
  BLOCK(valid) {
    require(xs.size() == 3);
    for_each_i(xs, [&](auto&, auto& e) {
      require(e >= 0);
      require(e <= 10);
    });
    unique(xs);
    require(array_sum(xs) == 12);
    require(array_min(xs) >= 0);
    require(array_max(xs) <= 10);
  }
  FUZZY_END
};
```

可用的整数数组归约包括：

```cpp
require(array_sum(xs) == 12);
require(array_product(xs) == 24);
require(array_and(xs) == 1);
require(array_or(xs) == 1);
require(array_xor(xs) == 0);
require(array_min(xs) >= 0);
require(array_max(xs) <= 10);
```

`array_and`、`array_or` 和 `array_xor` 按元素是否为 0 做布尔归约，结果为 0 或 1。

归约可以带 C++ lambda 形式的元素转换表达式：

```cpp
require(array_sum(xs, [](auto&, auto& e) -> fuzzy::Expr& { return e * 2; }) == 24);
```

映射可以使用 `fuzzy::Dict`：

```cpp
#include "fuzzy/Dict.h"

struct Table {
  fuzzy::Dict<int, int> kv;

  FUZZY_META(Table, void, FUZZY_RAND(kv))
  BLOCK(valid) {
    require(kv.size() == 3);
    for_each_kv(kv, [&](auto&, auto& k, auto& v) {
      require(v >= k);
    });
  }
  FUZZY_END
};
```

## 枚举

`enum class` 可以作为随机字段使用。建议显式约束有效取值范围。

```cpp
enum class Kind { A = 0, B = 1, C = 2 };

struct Item {
  Kind kind{Kind::A};

  FUZZY_META(Item, void, FUZZY_RAND(kind))
  BLOCK(valid) {
    inside(kind, Kind::A, Kind::B, Kind::C);
  }
  FUZZY_END
};
```

## 整数位 Helper

对非负整数编码的位字段，可以使用 helper 表达常见位约束：

```cpp
require(x >= 0);
require(x <= 15);
require(bit_select(x, 2) == 1);
require(bit_slice(x, 3, 1) == 5);
require(y == bit_concat(hi, 2, lo));
require(wildcard_eq(x, 0b1010, 0b1110));
require(wildcard_ne(x, 0b0010, 0b1110));
```

这些 helper 使用整数算术建模位约束，适合 2-state、非负整数场景。

## 约束和随机化开关

关闭某个随机字段后，该字段保持当前值参与求解：

```cpp
Packet pkt{.kind = 1, .len = 0};
FUZZY_RAND_MODE(pkt, len) = false;
bool ok = fuzzy::randomize(pkt);
```

需要重新启用时：

```cpp
FUZZY_RAND_MODE(pkt, len) = true;
```

命名约束块可以按名字开关：

```cpp
pkt.fuzzy_constraint_mode("valid", 0);
pkt.fuzzy_constraint_mode("valid", 1);
```

## 随机化钩子

如果类型定义了以下成员函数，随机化时会自动调用：

```cpp
void fuzzy_pre_randomize();
void fuzzy_post_randomize();
```

`fuzzy_pre_randomize` 在求解前运行；`fuzzy_post_randomize` 只在随机化成功后运行。

## 语义差异

fuzzy 的目标是提供接近 SystemVerilog constrained random 的 C++ 写法，但以下能力不是逐字等价：

- `soft` 的丢弃顺序。
- `dist` 的精确加权分布。
- `solve_before` 对随机稳定性和分布的影响。
- 数组归约使用 C++ lambda 表达 `with`，不是 SystemVerilog 原语法。
- 位 helper 使用整数编码，不是完整 SystemVerilog bit-vector / 4-state 语义。
- `randc` 与复杂可行域组合时的周期行为。

依赖这些语义时，请参考 [`semantic_differences.md`](semantic_differences.md) 并补充面向场景的测试。
