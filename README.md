# Helix.Testing

Unit test, benchmark and fuzz framework for the Helix repositories. No dependencies besides the C++
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

Only the `KeepRunning` loop is timed, so setup before it is free. Each benchmark is scaled until one
run takes about 100 ms, then the best of three runs is
reported as ns/op and ns/item.

## Fuzzing

```cpp
#include <Helix/Testing/Fuzz.h>

HX_FUZZ(PngDecoder)
{
    Array<u8> bytes = SamplePng();
    usize size = bytes.Size();
    bytes.Resize(size + 64);
    bytes.Resize(round.Mutate(bytes.Data(), size, bytes.Size()));
    round.Step("decode");
    return DecodePng(bytes) ? 1 : 0;
}
```

A round is reproducible from its number: `FuzzRound` makes the same random choices and the same
mutations (bit flips, interesting bytes, copied runs, 16 and 32 bit values in both byte orders,
inserts, deletes, truncation) every time. Round n runs target n % count. The return value counts
the items the round got through.

Runner options: `--seconds=<s>` or `--rounds=<n>`, `--first=<round>`, `--target=<name>`,
`--progress=<file>` (the current round and target, written once a second and on a crash).
A failed check, an abort, a crash or a round that runs for a minute prints the round, the target,
the command that runs it again and the call stack (Windows and Linux). `HELIX_FUZZ_TRACE=1` prints
every `round.Step`.

## Layout

| Path | Contents |
|---|---|
| `Source/Helix/Testing/` | `Test.h`, `Bench.h`, `Fuzz.h` and their implementation |
| `Source/Main/` | Runner entry points for `Helix::TestMain`, `Helix::BenchMain` and `Helix::FuzzMain` |
| `Tests/`, `Benchmarks/`, `Fuzz/` | Self tests, overhead benchmarks and a sample fuzz target |
| `Tools/FuzzSupervisor.py` | Long fuzz runs: finds the exact round of each crash or hang (also AddressSanitizer reports), keeps its report and goes on from the next round |

## CMake

`helix_add_test_suite(name DEPS ...)` builds every file in `Tests/` against `Helix::TestMain`
and registers it with CTest. `helix_add_benchmark_suite(name DEPS ...)` does the same for
`Benchmarks/` with `Helix::BenchMain`. `helix_add_fuzz_suite(name DEPS ... [ROUNDS n])` builds
`Fuzz/` against `Helix::FuzzMain` and registers a short run of n rounds (40 by default) with CTest.

## License

GPL-3.0, see `LICENSE`.
