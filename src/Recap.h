#pragma once

#include <cstdint>
#include <string>

namespace Recap
{
    struct Options
    {
        bool showQuest{ true };
        bool showLocation{ true };
        bool showSnapshot{ true };
    };

    // a_secondsAway < 0 means "unknown" (save made before the mod): the line is omitted.
    // Returns an empty string if there is nothing to show (no player).
    std::string Build(std::int64_t a_secondsAway, const Options& a_options);
}
