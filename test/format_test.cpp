// Host self-check: clang++ -std=c++20 -Isrc test/format_test.cpp src/Format.cpp -o /tmp/ft && /tmp/ft
#undef NDEBUG
#include "Format.h"

#include <cassert>
#include <cstdio>

int main()
{
    using namespace Format;

    assert(Ago(-5) == "0 minutes");
    assert(Ago(59) == "0 minutes");
    assert(Ago(60) == "1 minute");
    assert(Ago(3599) == "59 minutes");
    assert(Ago(3600) == "1 hour");
    assert(Ago(47 * 3600) == "47 hours");
    assert(Ago(48 * 3600) == "2 days");
    assert(Ago(12 * 86400 + 5) == "12 days");

    assert(Thousands(0) == "0");
    assert(Thousands(999) == "999");
    assert(Thousands(1240) == "1,240");
    assert(Thousands(1234567) == "1,234,567");
    assert(Thousands(-1240) == "-1,240");

    auto lookup = [](std::string_view a) -> std::optional<std::string> {
        if (a == "Jarl") {
            return "Balgruuf";
        }
        return std::nullopt;
    };
    assert(ResolveTags("Speak to <Alias=Jarl>", lookup) == "Speak to Balgruuf");
    assert(ResolveTags("Speak to <Alias.ShortName=Jarl> now", lookup) == "Speak to Balgruuf now");
    assert(ResolveTags("Bring the <Alias=Item> to <Alias=Jarl>", lookup) == "Bring the to Balgruuf");
    assert(ResolveTags("Collect <Global=Count> pelts", lookup) == "Collect pelts");
    assert(ResolveTags("No tags", lookup) == "No tags");
    assert(ResolveTags("Broken <tag", lookup) == "Broken <tag");
    assert(ResolveTags("", lookup).empty());

    std::puts("format_test: OK");
    return 0;
}
