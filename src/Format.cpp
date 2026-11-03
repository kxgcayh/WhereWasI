#include "Format.h"

namespace Format
{
    namespace
    {
        std::string Unit(std::int64_t a_n, const char* a_unit)
        {
            return std::to_string(a_n) + " " + a_unit + (a_n == 1 ? "" : "s");
        }
    }

    std::string Ago(std::int64_t a_seconds)
    {
        if (a_seconds < 0) {
            a_seconds = 0;
        }
        const auto minutes = a_seconds / 60;
        if (minutes < 60) {
            return Unit(minutes, "minute");
        }
        const auto hours = minutes / 60;
        if (hours < 48) {
            return Unit(hours, "hour");
        }
        return Unit(hours / 24, "day");
    }

    std::string Thousands(std::int64_t a_value)
    {
        const bool negative = a_value < 0;
        auto digits = std::to_string(negative ? -a_value : a_value);
        for (auto i = static_cast<std::ptrdiff_t>(digits.size()) - 3; i > 0; i -= 3) {
            digits.insert(static_cast<std::size_t>(i), ",");
        }
        return negative ? "-" + digits : digits;
    }

    std::optional<std::string> ResolveTags(std::string_view a_text,
        const std::function<std::optional<std::string>(std::string_view)>& a_lookup)
    {
        std::string out;
        std::size_t pos = 0;
        while (pos < a_text.size()) {
            const auto open = a_text.find('<', pos);
            const auto close = open == std::string_view::npos ? open : a_text.find('>', open);
            if (close == std::string_view::npos) {
                out.append(a_text.substr(pos));
                break;
            }
            out.append(a_text.substr(pos, open - pos));

            const auto tag = a_text.substr(open + 1, close - open - 1);
            const auto eq = tag.find('=');
            if (tag.starts_with("Alias") && eq != std::string_view::npos) {
                auto name = a_lookup(tag.substr(eq + 1));
                if (!name || name->empty()) {
                    return std::nullopt;
                }
                out.append(*name);
            }
            pos = close + 1;
        }

        std::string collapsed;
        for (const char c : out) {
            if (c == ' ' && !collapsed.empty() && collapsed.back() == ' ') {
                continue;
            }
            collapsed.push_back(c);
        }
        while (!collapsed.empty() && collapsed.back() == ' ') {
            collapsed.pop_back();
        }
        if (!collapsed.empty() && collapsed.front() == ' ') {
            collapsed.erase(0, 1);
        }
        return collapsed;
    }
}
