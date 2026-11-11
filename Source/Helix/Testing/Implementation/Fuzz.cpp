#include <Helix/Testing/Fuzz.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
#define HX_FUZZ_WATCHDOG 0
#else
#define HX_FUZZ_WATCHDOG 1
#include <thread>
#endif

#ifdef _WIN32
#include <windows.h>

#include <dbghelp.h>
#ifdef _MSC_VER
#pragma comment(lib, "dbghelp.lib")
#endif
#elif defined(__linux__) && !defined(__ANDROID__)
#include <execinfo.h>
#include <pthread.h>
#include <unistd.h>
#define HX_FUZZ_BACKTRACE 1
#endif

namespace helix::testing
{
    namespace
    {
        FuzzTarget* first = nullptr;
        FuzzTarget* last = nullptr;

        bool Tracing()
        {
            static const bool trace = std::getenv("HELIX_FUZZ_TRACE") != nullptr;
            return trace;
        }

        // What the crash handlers report: set before every round.
        std::atomic<std::uint64_t> currentRound{0};
        std::atomic<const char*> currentTarget{nullptr};
        const char* program = "fuzzer";
        const char* progressPath = nullptr;

        void WriteProgress()
        {
            if (!progressPath)
                return;
            if (std::FILE* f = std::fopen(progressPath, "wb"))
            {
                const char* target = currentTarget.load();
                std::fprintf(f, "%llu %s\n", static_cast<unsigned long long>(currentRound.load()), target ? target : "");
                std::fclose(f);
            }
        }

#ifdef _WIN32
        void PrintFrames(void* const* frames, unsigned count)
        {
            HANDLE process = GetCurrentProcess();
            SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
            SymInitialize(process, nullptr, TRUE);
            alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO) + 256];
            SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(storage);
            for (unsigned i = 0; i < count; i++)
            {
                symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
                symbol->MaxNameLen = 255;
                DWORD64 address = DWORD64(frames[i]), offset = 0;
                IMAGEHLP_LINE64 location{};
                location.SizeOfStruct = sizeof(location);
                DWORD column = 0;
                bool named = SymFromAddr(process, address, &offset, symbol);
                bool placed = SymGetLineFromAddr64(process, address, &column, &location);
                std::fprintf(stderr, "    %s  %s:%lu\n", named ? symbol->Name : "?", placed ? location.FileName : "?", placed ? location.LineNumber : 0ul);
            }
        }

        // The thread that runs the rounds, for the stack of a hanging round.
        HANDLE mainThread = nullptr;
#elif defined(HX_FUZZ_BACKTRACE)
        pthread_t mainThread;
        std::atomic<bool> stackPrinted{false};

        void OnStackRequest(int)
        {
            void* frames[48];
            int count = backtrace(frames, 48);
            backtrace_symbols_fd(frames, count, STDERR_FILENO);
            stackPrinted.store(true);
        }
#endif

        void PrintStack()
        {
#ifdef _WIN32
            void* frames[48];
            USHORT count = CaptureStackBackTrace(2, 48, frames, nullptr);
            PrintFrames(frames, count);
#elif defined(HX_FUZZ_BACKTRACE)
            void* frames[48];
            int count = backtrace(frames, 48);
            backtrace_symbols_fd(frames, count, STDERR_FILENO);
#endif
        }

#if HX_FUZZ_WATCHDOG
        // The stack of the thread that runs the rounds, printed from the watchdog while that thread hangs.
        void PrintHangingStack()
        {
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
            if (!mainThread || SuspendThread(mainThread) == DWORD(-1))
                return;
            CONTEXT context{};
            context.ContextFlags = CONTEXT_FULL;
            if (GetThreadContext(mainThread, &context))
            {
                SymInitialize(GetCurrentProcess(), nullptr, TRUE);
                STACKFRAME64 frame{};
                frame.AddrPC.Offset = context.Rip;
                frame.AddrPC.Mode = AddrModeFlat;
                frame.AddrFrame.Offset = context.Rbp;
                frame.AddrFrame.Mode = AddrModeFlat;
                frame.AddrStack.Offset = context.Rsp;
                frame.AddrStack.Mode = AddrModeFlat;
                void* frames[48];
                unsigned count = 0;
                while (count < 48 && StackWalk64(IMAGE_FILE_MACHINE_AMD64, GetCurrentProcess(), mainThread, &frame, &context, nullptr,
                                                 SymFunctionTableAccess64, SymGetModuleBase64, nullptr) &&
                       frame.AddrPC.Offset != 0)
                    frames[count++] = reinterpret_cast<void*>(frame.AddrPC.Offset);
                PrintFrames(frames, count);
            }
#elif defined(HX_FUZZ_BACKTRACE)
            pthread_kill(mainThread, SIGUSR2);
            for (int i = 0; i < 40 && !stackPrinted.load(); i++)
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
#endif
        }
#endif

        void Report(const char* what)
        {
            const char* target = currentTarget.load();
            unsigned long long round = currentRound.load();
            std::fprintf(stderr, "fuzz round %llu of %s: %s\n    again: %s --first=%llu --rounds=1 --target=%s\n", round, target ? target : "?", what,
                         program, round, target ? target : "?");
            PrintStack();
            std::fflush(stderr);
            WriteProgress();
        }

        void OnSignal(int code)
        {
            const char* what = code == SIGABRT ? "abort (a failed check)" : code == SIGSEGV ? "segmentation fault" : code == SIGFPE ? "arithmetic fault" : "illegal instruction";
            Report(what);
            std::signal(code, SIG_DFL);
            std::raise(code);
        }

#ifdef _WIN32
        LONG WINAPI OnException(EXCEPTION_POINTERS* info)
        {
            char what[64];
            std::snprintf(what, sizeof(what), "exception 0x%08lX", info->ExceptionRecord->ExceptionCode);
            Report(what);
            return EXCEPTION_EXECUTE_HANDLER;
        }
#endif

        void InstallHandlers()
        {
#ifdef _WIN32
            // Room to report a stack overflow from its own handler, and no dialog for abort().
            ULONG reserve = 64 * 1024;
            SetThreadStackGuarantee(&reserve);
            SetUnhandledExceptionFilter(&OnException);
            SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#ifdef _MSC_VER
            _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
            std::signal(SIGABRT, &OnSignal);
            DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &mainThread, 0, FALSE, DUPLICATE_SAME_ACCESS);
#else
            for (int code : {SIGABRT, SIGSEGV, SIGFPE, SIGILL})
                std::signal(code, &OnSignal);
#endif
#ifdef HX_FUZZ_BACKTRACE
            mainThread = pthread_self();
            std::signal(SIGUSR2, &OnStackRequest);
#endif
        }

        const char* Arg(int argc, char** argv, const char* key)
        {
            std::size_t n = std::strlen(key);
            for (int i = 1; i < argc; i++)
                if (std::strncmp(argv[i], key, n) == 0)
                    return argv[i] + n;
            return nullptr;
        }
    }

    FuzzRound::FuzzRound(std::uint64_t number) : number_(number), state_(number * 0x9E3779B97F4A7C15ull + 1)
    {
        for (int i = 0; i < 4; i++)
            Next();
    }

    std::uint32_t FuzzRound::Next()
    {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 7;
        state_ ^= state_ << 17;
        return std::uint32_t(state_ >> 16);
    }

    std::uint32_t FuzzRound::Below(std::uint32_t count) { return count ? Next() % count : 0; }

    std::size_t FuzzRound::Index(std::size_t size) { return size ? std::size_t(((std::uint64_t(Next()) << 32) | Next()) % size) : 0; }

    std::size_t FuzzRound::Mutate(std::uint8_t* bytes, std::size_t size, std::size_t capacity)
    {
        static constexpr std::uint8_t kBytes[] = {0x00, 0x01, 0x7F, 0x80, 0xFE, 0xFF};
        static constexpr std::uint16_t kShorts[] = {0x0000, 0x0001, 0x7FFF, 0x8000, 0xFFFF, 0x0100};
        std::uint32_t edits = 1 + Below(16);
        for (std::uint32_t e = 0; e < edits; e++)
        {
            if (size == 0)
            {
                if (capacity == 0)
                    break;
                bytes[size++] = std::uint8_t(Next());
                continue;
            }
            std::size_t at = Index(size);
            switch (Below(9))
            {
            case 0: bytes[at] ^= std::uint8_t(1u << Below(8)); break;
            case 1: bytes[at] = std::uint8_t(Next()); break;
            case 2: bytes[at] = kBytes[Below(6)]; break;
            case 3:
            {
                std::size_t from = Index(size), count = 1 + Below(32);
                for (std::size_t i = 0; i < count && at + i < size && from + i < size; i++)
                    bytes[at + i] = bytes[from + i];
                break;
            }
            case 4:
            case 5:
            {
                // A count, offset or index: small, at a sign boundary, the size of the input, or random.
                bool wide = Below(2) == 0;
                std::uint32_t values[] = {kShorts[Below(6)], 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu, std::uint32_t(size), std::uint32_t(size + 1), Next()};
                std::uint32_t value = values[Below(7)];
                std::size_t width = wide ? 4 : 2;
                bool big = Below(2) == 0;
                for (std::size_t i = 0; i < width && at + i < size; i++)
                    bytes[at + i] = std::uint8_t(value >> (8 * (big ? width - 1 - i : i)));
                break;
            }
            case 6:
            {
                std::size_t count = 1 + Below(16);
                if (size + count > capacity)
                    count = capacity - size;
                if (count == 0)
                    break;
                std::memmove(bytes + at + count, bytes + at, size - at);
                for (std::size_t i = 0; i < count; i++)
                    bytes[at + i] = Below(2) ? std::uint8_t(Next()) : bytes[Index(size)];
                size += count;
                break;
            }
            case 7:
            {
                std::size_t count = 1 + Below(16);
                if (count > size - at)
                    count = size - at;
                std::memmove(bytes + at, bytes + at + count, size - at - count);
                size -= count;
                break;
            }
            default:
                if (Below(8) == 0)
                    size = at + 1;
                break;
            }
        }
        return size;
    }

    void FuzzRound::Step(const char* what, const char* detail) const
    {
        if (!Tracing())
            return;
        std::printf("  %s %s\n", what, detail ? detail : "");
        std::fflush(stdout);
    }

    FuzzRegistrar::FuzzRegistrar(FuzzTarget& target)
    {
        if (last)
            last->next = &target;
        else
            first = &target;
        last = &target;
    }

    int RunFuzz(int argc, char** argv)
    {
        program = argc > 0 ? argv[0] : "fuzzer";
        const char* secondsArg = Arg(argc, argv, "--seconds=");
        const char* roundsArg = Arg(argc, argv, "--rounds=");
        const char* firstArg = Arg(argc, argv, "--first=");
        const char* only = Arg(argc, argv, "--target=");
        progressPath = Arg(argc, argv, "--progress=");
        if (!secondsArg && !roundsArg)
        {
            std::fprintf(stderr, "usage: %s (--seconds=S | --rounds=N) [--first=N] [--target=name] [--progress=file]\n", program);
            return 2;
        }
        FuzzTarget* targets[256];
        std::uint32_t count = 0;
        for (FuzzTarget* t = first; t && count < 256; t = t->next)
            if (!only || std::strcmp(only, t->name) == 0)
                targets[count++] = t;
        if (count == 0)
        {
            std::fprintf(stderr, "no fuzz target%s%s\n", only ? " named " : "", only ? only : "");
            return 2;
        }
        double seconds = secondsArg ? std::atof(secondsArg) : 1e300;
        std::uint64_t rounds = roundsArg ? std::strtoull(roundsArg, nullptr, 10) : ~0ull;
        std::uint64_t round = firstArg ? std::strtoull(firstArg, nullptr, 10) : 0;
        InstallHandlers();

        std::atomic<bool> done{false};
#if HX_FUZZ_WATCHDOG
        // Writes the progress file once a second and stops a round that runs for a minute.
        std::thread watchdog([&] {
            std::uint64_t seen = ~0ull;
            auto since = std::chrono::steady_clock::now();
            while (!done.load())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
                std::uint64_t now = currentRound.load();
                if (now != seen)
                {
                    seen = now;
                    since = std::chrono::steady_clock::now();
                    WriteProgress();
                }
                else if (std::chrono::steady_clock::now() - since > std::chrono::seconds(60))
                {
                    WriteProgress();
                    std::fprintf(stderr, "fuzz round %llu of %s hangs\n    again: %s --first=%llu --rounds=1 --target=%s\n",
                                 static_cast<unsigned long long>(now), currentTarget.load(), program, static_cast<unsigned long long>(now), currentTarget.load());
                    PrintHangingStack();
                    std::fflush(stderr);
                    std::_Exit(3);
                }
            }
        });
#endif

        auto start = std::chrono::steady_clock::now(), reported = start;
        std::uint64_t ran = 0, items = 0;
        for (; ran < rounds; round++)
        {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - start).count();
            if (elapsed >= seconds)
                break;
            if (now - reported > std::chrono::seconds(60))
            {
                reported = now;
                std::printf("%llu rounds, %.0f s, %llu items\n", static_cast<unsigned long long>(ran), elapsed, static_cast<unsigned long long>(items));
                std::fflush(stdout);
            }
            FuzzTarget* target = targets[round % count];
            currentTarget.store(target->name);
            currentRound.store(round);
            FuzzRound fuzz(round);
            items += target->run(fuzz);
            ran++;
        }
        done.store(true);
#if HX_FUZZ_WATCHDOG
        watchdog.join();
#endif
        WriteProgress();
        double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::printf("done: %llu rounds of %u target%s in %.1f s without a crash or a hang, %llu items\n", static_cast<unsigned long long>(ran), count,
                    count == 1 ? "" : "s", elapsed, static_cast<unsigned long long>(items));
        return 0;
    }
}
