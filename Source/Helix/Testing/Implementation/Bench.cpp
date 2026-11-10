#include <Helix/Testing/Bench.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>

namespace helix::testing
{
    namespace
    {
        BenchCase* first = nullptr;
        BenchCase* last = nullptr;

        double RunOnce(BenchCase& bench, std::uint64_t iterations, std::uint64_t& items)
        {
            BenchState state(iterations);
            auto a = std::chrono::steady_clock::now();
            bench.run(state);
            auto b = std::chrono::steady_clock::now();
            items = state.ItemsPerIteration();
            double loop = state.ElapsedNanoseconds();
            return loop >= 0.0 ? loop : std::chrono::duration<double, std::nano>(b - a).count();
        }
    }

    void BenchState::Start() { start_ = std::chrono::steady_clock::now().time_since_epoch().count(); }

    void BenchState::Stop()
    {
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        elapsed_ = std::chrono::duration<double, std::nano>(now - std::chrono::steady_clock::duration(start_)).count();
    }

    BenchRegistrar::BenchRegistrar(BenchCase& bench)
    {
        if (last)
            last->next = &bench;
        else
            first = &bench;
        last = &bench;
    }

    int RunBenchmarks(int argc, char** argv)
    {
        const char* filter = nullptr;
        for (int i = 1; i < argc; i++)
            if (std::strncmp(argv[i], "--filter=", 9) == 0)
                filter = argv[i] + 9;

        constexpr double targetNs = 100e6;
        std::printf("%-40s %14s %14s %16s\n", "benchmark", "ns/op", "ns/item", "iterations");
        for (BenchCase* b = first; b; b = b->next)
        {
            if (filter && !std::strstr(b->name, filter))
                continue;
            std::uint64_t iterations = 1, items = 1;
            double ns = RunOnce(*b, iterations, items);
            while (ns < targetNs && iterations < (1ull << 40))
            {
                double scale = ns > 0 ? targetNs / ns : 100.0;
                iterations = std::max<std::uint64_t>(iterations * 2, std::uint64_t(double(iterations) * std::min(scale * 1.2, 100.0)));
                ns = RunOnce(*b, iterations, items);
            }
            double best = ns;
            for (int sample = 0; sample < 2; sample++)
                best = std::min(best, RunOnce(*b, iterations, items));
            double perOp = best / double(iterations);
            std::printf("%-40s %14.2f %14.3f %16llu\n", b->name, perOp, perOp / double(items ? items : 1), static_cast<unsigned long long>(iterations));
        }
        return 0;
    }
}
