#pragma once

// Pure string helpers: no CommonLib, so test/format_test.cpp can build them on the host.
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace Format
{
    // "5 minutes", "1 hour", "12 days". Negative input (clock skew) counts as 0.
    std::string Ago(std::int64_t a_seconds);

    // 1240 -> "1,240"
    std::string Thousands(std::int64_t a_value);

    // Replaces <Alias=Name> / <Alias.ShortName=Name> via a_lookup; other tags are removed and the double
    // spaces that leaves are collapsed. Returns nullopt if any alias cannot be filled ("Bring the  to X").
    std::optional<std::string> ResolveTags(std::string_view a_text,
        const std::function<std::optional<std::string>(std::string_view)>& a_lookup);
}
