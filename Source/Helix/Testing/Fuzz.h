#pragma once

#include <cstddef>
#include <cstdint>

namespace helix::testing
{
    // The random choices of one fuzz round: the same round number makes the same choices every time.
    class FuzzRound
    {
    public:
        explicit FuzzRound(std::uint64_t number);

        std::uint64_t Number() const { return number_; }
        std::uint32_t Next();
        // A value below count (0 when count is 0).
        std::uint32_t Below(std::uint32_t count);

        // Changes a few of bytes[0, size): bit flips, interesting bytes, runs copied from elsewhere, 16 and 32 bit values
        // in both byte orders, inserted and removed bytes (up to capacity) and truncation. Returns the new size.
        std::size_t Mutate(std::uint8_t* bytes, std::size_t size, std::size_t capacity);

        // With HELIX_FUZZ_TRACE set it prints each step, to find the one a round fails in.
        void Step(const char* what, const char* detail = nullptr) const;

    private:
        std::size_t Index(std::size_t size);

        std::uint64_t number_;
        std::uint64_t state_;
    };

    // A fuzz target: runs one round on mutated input and returns how many items it got through (for the progress line).
    // It must never crash, hang or fail a check, whatever the input.
    struct FuzzTarget
    {
        const char* name;
        std::uint64_t (*run)(FuzzRound&);
        FuzzTarget* next;
    };

    struct FuzzRegistrar
    {
        explicit FuzzRegistrar(FuzzTarget& target);
    };

    // usage: <fuzzer> (--seconds=S | --rounds=N) [--first=N] [--target=name] [--progress=file]
    // Round n runs target n % count. A failed check, an abort, a crash or a round that takes a minute prints the round,
    // the target, the command that runs it again and the call stack.
    int RunFuzz(int argc, char** argv);
}

#define HX_FUZZ(name)                                                                                    \
    static std::uint64_t hx_fuzz_##name(::helix::testing::FuzzRound& round);                             \
    static ::helix::testing::FuzzTarget hx_fuzz_target_##name{#name, &hx_fuzz_##name, nullptr};         \
    static ::helix::testing::FuzzRegistrar hx_fuzz_registrar_##name{hx_fuzz_target_##name};             \
    static std::uint64_t hx_fuzz_##name(::helix::testing::FuzzRound& round)
