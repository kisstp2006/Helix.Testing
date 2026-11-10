#include <Helix/Testing/Test.h>

using helix::testing::Describe;

namespace
{
    enum class Color { Red = 3 };

    int counter = 0;
}

HX_TEST(Describe, Integers)
{
    char buffer[64];
    Describe(-42, buffer, sizeof(buffer));
    HX_CHECK_STR(buffer, "-42");
    Describe(255u, buffer, sizeof(buffer));
    HX_CHECK_STR(buffer, "255 (0xff)");
}

HX_TEST(Describe, OtherTypes)
{
    char buffer[64];
    Describe(true, buffer, sizeof(buffer));
    HX_CHECK_STR(buffer, "true");
    Describe(0.5f, buffer, sizeof(buffer));
    HX_CHECK_STR(buffer, "0.5");
    Describe("abc", buffer, sizeof(buffer));
    HX_CHECK_STR(buffer, "\"abc\"");
    Describe(Color::Red, buffer, sizeof(buffer));
    HX_CHECK_STR(buffer, "3");
}

HX_TEST(Registration, RunsInDeclarationOrderFirst)
{
    HX_CHECK_EQ(counter, 0);
    counter = 1;
}

HX_TEST(Registration, RunsInDeclarationOrderSecond)
{
    HX_CHECK_EQ(counter, 1);
}

HX_TEST(Checks, EqualAndNear)
{
    HX_CHECK_EQ(2 + 2, 4);
    HX_CHECK_NEAR(0.1 + 0.2, 0.3, 1e-12);
    HX_REQUIRE(counter == 1);
}
