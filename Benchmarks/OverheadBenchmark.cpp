#include <Helix/Testing/Bench.h>

HX_BENCH(EmptyLoop)
{
    while (state.KeepRunning())
        helix::testing::ClobberMemory();
}

HX_BENCH(SumArray)
{
    int values[1024];
    for (int i = 0; i < 1024; i++)
        values[i] = i;
    state.SetItemsPerIteration(1024);
    while (state.KeepRunning())
    {
        long long sum = 0;
        for (int v : values)
            sum += v;
        helix::testing::DoNotOptimize(sum);
    }
}
