#include <Helix/Testing/Fuzz.h>
#include <Helix/Testing/Test.h>

#include <cstring>

using helix::testing::FuzzRound;

HX_TEST(Fuzz, RoundsRepeatTheirChoices)
{
    FuzzRound a(17), b(17), c(18);
    std::uint32_t x = a.Next(), y = b.Next();
    HX_CHECK_EQ(x, y);
    HX_CHECK(c.Next() != x);
}

HX_TEST(Fuzz, MutationsStayInsideTheBuffer)
{
    std::uint8_t source[64], bytes[64 + 32];
    for (int i = 0; i < 64; i++)
        source[i] = std::uint8_t(i * 7);
    int changed = 0, resized = 0;
    for (std::uint64_t round = 0; round < 2000; round++)
    {
        std::memcpy(bytes, source, 64);
        FuzzRound fuzz(round);
        std::size_t size = fuzz.Mutate(bytes, 64, sizeof(bytes));
        HX_CHECK(size <= sizeof(bytes));
        changed += size != 64 || std::memcmp(bytes, source, 64) != 0;
        resized += size != 64;
        // The same round makes the same bytes.
        std::uint8_t again[64 + 32];
        std::memcpy(again, source, 64);
        FuzzRound repeat(round);
        HX_CHECK_EQ(repeat.Mutate(again, 64, sizeof(again)), size);
        HX_CHECK(std::memcmp(again, bytes, size) == 0);
    }
    HX_CHECK(changed > 1900);
    HX_CHECK(resized > 100);
    // Empty input grows when there is room and stays empty when there is none.
    FuzzRound fuzz(1);
    HX_CHECK(fuzz.Mutate(bytes, 0, sizeof(bytes)) > 0);
    HX_CHECK_EQ(fuzz.Mutate(bytes, 0, 0), std::size_t(0));
}
