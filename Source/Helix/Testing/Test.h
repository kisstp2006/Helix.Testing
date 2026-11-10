#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

namespace helix::testing
{
    struct TestCase
    {
        const char* suite;
        const char* name;
        void (*run)();
        const char* file;
        int line;
        TestCase* next;
    };

    struct Registrar
    {
        explicit Registrar(TestCase& test);
    };

    void ReportFailure(const char* file, int line, const char* expression, const char* detail);
    int RunTests(int argc, char** argv);

    template <typename T>
    void Describe(const T& value, char* out, std::size_t size)
    {
        using U = std::remove_cvref_t<T>;
        if constexpr (std::is_same_v<U, bool>)
            std::snprintf(out, size, "%s", value ? "true" : "false");
        else if constexpr (std::is_same_v<U, char>)
            std::snprintf(out, size, "'%c' (%d)", value, int(value));
        else if constexpr (std::is_enum_v<U>)
            std::snprintf(out, size, "%lld", static_cast<long long>(value));
        else if constexpr (std::is_integral_v<U> && std::is_signed_v<U>)
            std::snprintf(out, size, "%lld", static_cast<long long>(value));
        else if constexpr (std::is_integral_v<U>)
            std::snprintf(out, size, "%llu (0x%llx)", static_cast<unsigned long long>(value), static_cast<unsigned long long>(value));
        else if constexpr (std::is_floating_point_v<U>)
            std::snprintf(out, size, "%.9g", static_cast<double>(value));
        else if constexpr (std::is_convertible_v<U, const char*>)
            std::snprintf(out, size, "\"%s\"", static_cast<const char*>(value));
        else if constexpr (std::is_pointer_v<U>)
            std::snprintf(out, size, "%p", static_cast<const void*>(value));
        else
            std::snprintf(out, size, "(value)");
    }

    template <typename A, typename B>
    void CheckEqual(const char* file, int line, const char* expression, const A& a, const B& b)
    {
        if (a == b)
            return;
        char left[96], right[96], detail[224];
        Describe(a, left, sizeof(left));
        Describe(b, right, sizeof(right));
        std::snprintf(detail, sizeof(detail), "left %s, right %s", left, right);
        ReportFailure(file, line, expression, detail);
    }

    inline void CheckString(const char* file, int line, const char* expression, const char* a, const char* b)
    {
        if (a && b && std::strcmp(a, b) == 0)
            return;
        char detail[512];
        std::snprintf(detail, sizeof(detail), "left \"%s\", right \"%s\"", a ? a : "(null)", b ? b : "(null)");
        ReportFailure(file, line, expression, detail);
    }
}

#define HX_TEST_JOIN2(a, b) a##b
#define HX_TEST_JOIN(a, b) HX_TEST_JOIN2(a, b)

#define HX_TEST(suite, name)                                                                                     \
    static void HX_TEST_JOIN(hx_test_, HX_TEST_JOIN(suite, HX_TEST_JOIN(_, name)))();                           \
    static ::helix::testing::TestCase HX_TEST_JOIN(hx_case_, HX_TEST_JOIN(suite, HX_TEST_JOIN(_, name))){       \
        #suite, #name, &HX_TEST_JOIN(hx_test_, HX_TEST_JOIN(suite, HX_TEST_JOIN(_, name))), __FILE__, __LINE__, \
        nullptr};                                                                                                \
    static ::helix::testing::Registrar HX_TEST_JOIN(hx_reg_, HX_TEST_JOIN(suite, HX_TEST_JOIN(_, name))){       \
        HX_TEST_JOIN(hx_case_, HX_TEST_JOIN(suite, HX_TEST_JOIN(_, name)))};                                    \
    static void HX_TEST_JOIN(hx_test_, HX_TEST_JOIN(suite, HX_TEST_JOIN(_, name)))()

#define HX_CHECK(expr) ((expr) ? (void)0 : ::helix::testing::ReportFailure(__FILE__, __LINE__, #expr, nullptr))

#define HX_REQUIRE(expr)                                                         \
    do                                                                           \
    {                                                                            \
        if (!(expr))                                                             \
        {                                                                        \
            ::helix::testing::ReportFailure(__FILE__, __LINE__, #expr, nullptr); \
            return;                                                              \
        }                                                                        \
    } while (0)

#define HX_CHECK_EQ(a, b) ::helix::testing::CheckEqual(__FILE__, __LINE__, #a " == " #b, (a), (b))
#define HX_CHECK_STR(a, b) ::helix::testing::CheckString(__FILE__, __LINE__, #a " == " #b, (a), (b))
#define HX_CHECK_NEAR(a, b, eps) HX_CHECK(((a) - (b)) <= (eps) && ((b) - (a)) <= (eps))
