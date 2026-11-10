#include <Helix/Testing/Test.h>

#include <chrono>
#include <cstdio>
#include <cstring>

namespace helix::testing
{
    namespace
    {
        TestCase* first = nullptr;
        TestCase* last = nullptr;
        int failuresInTest = 0;
        char firstFailure[768];

        struct Result
        {
            TestCase* test;
            bool passed;
            double ms;
            char message[768];
        };

        const char* Arg(int argc, char** argv, const char* key)
        {
            std::size_t n = std::strlen(key);
            for (int i = 1; i < argc; i++)
                if (std::strncmp(argv[i], key, n) == 0)
                    return argv[i] + n;
            return nullptr;
        }

        bool Matches(const TestCase& t, const char* filter)
        {
            if (!filter || !*filter)
                return true;
            char full[256];
            std::snprintf(full, sizeof(full), "%s.%s", t.suite, t.name);
            return std::strstr(full, filter) != nullptr;
        }

        void WriteXmlEscaped(std::FILE* f, const char* s)
        {
            for (; *s; s++)
            {
                switch (*s)
                {
                case '<': std::fputs("&lt;", f); break;
                case '>': std::fputs("&gt;", f); break;
                case '&': std::fputs("&amp;", f); break;
                case '"': std::fputs("&quot;", f); break;
                default: std::fputc(*s, f);
                }
            }
        }

        void WriteJunit(const char* path, const Result* results, int count, int failed, double totalMs)
        {
            std::FILE* f = std::fopen(path, "wb");
            if (!f)
                return;
            std::fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<testsuites tests=\"%d\" failures=\"%d\" time=\"%.3f\">\n", count, failed, totalMs / 1000.0);
            for (int i = 0; i < count; i++)
            {
                const Result& r = results[i];
                std::fprintf(f, "  <testcase classname=\"%s\" name=\"%s\" time=\"%.3f\"", r.test->suite, r.test->name, r.ms / 1000.0);
                if (r.passed)
                {
                    std::fputs("/>\n", f);
                    continue;
                }
                std::fputs(">\n    <failure message=\"", f);
                WriteXmlEscaped(f, r.message);
                std::fputs("\"/>\n  </testcase>\n", f);
            }
            std::fputs("</testsuites>\n", f);
            std::fclose(f);
        }
    }

    Registrar::Registrar(TestCase& test)
    {
        if (last)
            last->next = &test;
        else
            first = &test;
        last = &test;
    }

    void ReportFailure(const char* file, int line, const char* expression, const char* detail)
    {
        char message[768];
        std::snprintf(message, sizeof(message), "%s(%d): check failed: %s%s%s", file, line, expression, detail ? " | " : "", detail ? detail : "");
        std::printf("    %s\n", message);
        if (failuresInTest++ == 0)
            std::memcpy(firstFailure, message, sizeof(message));
    }

    int RunTests(int argc, char** argv)
    {
        const char* filter = Arg(argc, argv, "--filter=");
        const char* junit = Arg(argc, argv, "--junit=");
        bool list = Arg(argc, argv, "--list") != nullptr;

        int count = 0;
        for (TestCase* t = first; t; t = t->next)
            if (Matches(*t, filter))
                count++;

        if (list)
        {
            for (TestCase* t = first; t; t = t->next)
                if (Matches(*t, filter))
                    std::printf("%s.%s\n", t->suite, t->name);
            return 0;
        }

        Result* results = new Result[count > 0 ? count : 1];
        int index = 0, failed = 0;
        auto start = std::chrono::steady_clock::now();
        for (TestCase* t = first; t; t = t->next)
        {
            if (!Matches(*t, filter))
                continue;
            failuresInTest = 0;
            auto a = std::chrono::steady_clock::now();
            t->run();
            auto b = std::chrono::steady_clock::now();
            Result& r = results[index++];
            r.test = t;
            r.passed = failuresInTest == 0;
            r.ms = std::chrono::duration<double, std::milli>(b - a).count();
            std::snprintf(r.message, sizeof(r.message), "%s", r.passed ? "" : firstFailure);
            std::printf("[%s] %s.%s (%.2f ms)\n", r.passed ? "  OK  " : " FAIL ", t->suite, t->name, r.ms);
            failed += r.passed ? 0 : 1;
        }
        double totalMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        std::printf("\n%d tests, %d passed, %d failed (%.1f ms)\n", count, count - failed, failed, totalMs);
        if (junit && *junit)
            WriteJunit(junit, results, count, failed, totalMs);
        delete[] results;
        return failed == 0 ? 0 : 1;
    }
}
