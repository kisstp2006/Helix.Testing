# Helix.Testing

Unit test and benchmark framework for the Helix repositories. No dependencies besides the C++
standard library and Helix.CMake.

## Tests

```cpp
#include <Helix/Testing/Test.h>

HX_TEST(Array, PushGrows)
{
    HX_CHECK_EQ(1 + 1, 2);
    HX_CHECK_STR("abc", "abc");
    HX_CHECK_NEAR(0.1 + 0.2, 0.3, 1e-12);
    HX_REQUIRE(true);
}
```

| Macro | Meaning |
|---|---|
| `HX_TEST(Suite, Name)` | Declares a test, run in declaration order |
| `HX_CHECK(expr)` | Records a failure and continues |
| `HX_REQUIRE(expr)` | Records a failure and leaves the test |
| `HX_CHECK_EQ(a, b)` | Equality with both values printed on failure |
| `HX_CHECK_STR(a, b)` | C string equality |
| `HX_CHECK_NEAR(a, b, eps)` | Absolute tolerance compare |

Runner options: `--filter=<text>` (matches `Suite.Name`), `--list`, `--junit=<file>`.

## Benchmarks

```cpp
#include <Helix/Testing/Bench.h>

HX_BENCH(SumArray)
{
    state.SetItemsPerIteration(1024);
    while (state.KeepRunning())
        helix::testing::DoNotOptimize(Sum(values));
}
```

Each benchmark is scaled until one run takes about 100 ms, then the best of three runs is
reported as ns/op and ns/item.

## Layout

| Path | Contents |
|---|---|
| `Source/Helix/Testing/` | `Test.h`, `Bench.h` and their implementation |
| `Source/Main/` | Runner entry points for `Helix::TestMain` and `Helix::BenchMain` |
| `Tests/`, `Benchmarks/` | Self tests and overhead benchmarks |

## CMake

`helix_add_test_suite(name DEPS ...)` builds every file in `Tests/` against `Helix::TestMain`
and registers it with CTest. `helix_add_benchmark_suite(name DEPS ...)` does the same for
`Benchmarks/` with `Helix::BenchMain`.

## License

GPL-3.0, see `LICENSE`.
