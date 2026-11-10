#include <Helix/Testing/Fuzz.h>

#include <cstring>

namespace
{
    // A reader of length prefixed records, written to survive any input: the runner's own target.
    std::uint64_t ReadRecords(const std::uint8_t* bytes, std::size_t size)
    {
        std::uint64_t records = 0;
        std::size_t at = 0;
        while (at + 2 <= size)
        {
            std::size_t length = std::size_t(bytes[at]) | std::size_t(bytes[at + 1]) << 8;
            at += 2;
            if (length > size - at)
                break;
            at += length;
            records++;
        }
        return records;
    }
}

HX_FUZZ(Records)
{
    std::uint8_t bytes[96] = {3, 0, 'a', 'b', 'c', 1, 0, 'x', 0, 0, 4, 0, 1, 2, 3, 4};
    std::size_t size = round.Mutate(bytes, 16, sizeof(bytes));
    round.Step("read", "records");
    return ReadRecords(bytes, size);
}
