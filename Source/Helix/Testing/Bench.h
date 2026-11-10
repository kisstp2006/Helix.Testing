#pragma once

#include <cstdint>

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

namespace helix::testing
{
    class BenchState
    {
    public:
        explicit BenchState(std::uint64_t iterations) : remaining_(iterations), iterations_(iterations) {}

        // Only the loop is timed: the clock starts at the first call and stops at the call that ends the loop.
        bool KeepRunning()
        {
            if (remaining_ == iterations_)
                Start();
            if (remaining_-- > 0)
                return true;
            Stop();
            return false;
        }
        double ElapsedNanoseconds() const { return elapsed_; }
        std::uint64_t Iterations() const { return iterations_; }
        void SetItemsPerIteration(std::uint64_t items) { items_ = items; }
        std::uint64_t ItemsPerIteration() const { return items_; }

    private:
        void Start();
        void Stop();

        std::uint64_t remaining_;
        std::uint64_t iterations_;
        std::uint64_t items_ = 1;
        std::int64_t start_ = 0;
        double elapsed_ = -1.0;
    };

    struct BenchCase
    {
        const char* name;
        void (*run)(BenchState&);
        BenchCase* next;
    };

    struct BenchRegistrar
    {
        explicit BenchRegistrar(BenchCase& bench);
    };

    int RunBenchmarks(int argc, char** argv);

    template <typename T>
    inline void DoNotOptimize(T const& value)
    {
#if defined(_MSC_VER) && !defined(__clang__)
        const volatile char* sink = reinterpret_cast<const volatile char*>(&value);
        (void)*sink;
#else
        asm volatile("" : : "r,m"(value) : "memory");
#endif
    }

    inline void ClobberMemory()
    {
#if defined(_MSC_VER) && !defined(__clang__)
        _ReadWriteBarrier();
#else
        asm volatile("" : : : "memory");
#endif
    }
}

#define HX_BENCH(name)                                                                            \
    static void hx_bench_##name(::helix::testing::BenchState& state);                            \
    static ::helix::testing::BenchCase hx_bench_case_##name{#name, &hx_bench_##name, nullptr};  \
    static ::helix::testing::BenchRegistrar hx_bench_reg_##name{hx_bench_case_##name};          \
    static void hx_bench_##name(::helix::testing::BenchState& state)
