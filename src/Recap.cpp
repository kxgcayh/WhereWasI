#include "PCH.h"

#include "Format.h"
#include "Recap.h"

namespace Recap
{
    namespace
    {
        std::optional<std::string> AliasName(RE::TESQuest* a_quest, std::string_view a_alias)
        {
            for (const auto* alias : a_quest->aliases) {
                if (!alias || std::string_view(alias->aliasName.c_str()) != a_alias) {
                    continue;
                }
                const auto ref = a_quest->GetAliasedRef(alias->aliasID).get();
                if (const char* name = ref ? ref->GetDisplayFullName() : nullptr; name && *name) {
                    return std::string(name);
                }
                return std::nullopt;
            }
            return std::nullopt;
        }

        std::string QuestLine(RE::PlayerCharacter* a_player)
        {
            // The journal's active quest is the one with TESQuest::IsActive(); its displayed
            // objectives sit in the player's objective list. The last match is the most recent.
            const RE::BGSQuestObjective* found = nullptr;
            for (const auto& inst : a_player->GetPlayerRuntimeData().objectives) {
                const auto* obj = inst.Objective;
                if (obj && obj->ownerQuest && obj->ownerQuest->IsActive() &&
                    inst.InstanceState == RE::QUEST_OBJECTIVE_STATE::kDisplayed) {
                    found = obj;
                }
            }
            if (!found) {
                return {};
            }

            auto* quest = found->ownerQuest;
            std::string line = "Quest: ";
            line += quest->GetFullName();
            const auto objective = Format::ResolveTags(found->displayText.c_str(),
                [quest](std::string_view a) { return AliasName(quest, a); });
            if (!objective.empty()) {
                line += " - " + objective;
            }
            return line;
        }

        std::string LocationLine(RE::PlayerCharacter* a_player)
        {
            const char* name = nullptr;
            if (const auto* loc = a_player->GetCurrentLocation()) {
                name = loc->GetFullName();
            }
            if ((!name || !*name) && a_player->GetParentCell()) {
                name = a_player->GetParentCell()->GetFullName();
            }
            return name && *name ? "Location: "s + name : std::string{};
        }

        std::string SnapshotLine(RE::PlayerCharacter* a_player)
        {
            std::string line = "Level " + std::to_string(a_player->GetLevel()) +
                               " | " + Format::Thousands(a_player->GetGoldAmount()) + " gold";

            std::int64_t bounty = 0;
            if (auto* data = RE::TESDataHandler::GetSingleton()) {
                for (const auto* faction : data->GetFormArray<RE::TESFaction>()) {
                    if (faction && (faction->data.flags & RE::FACTION_DATA::kTrackCrime)) {
                        bounty += std::max(faction->GetCrimeGold(), 0);
                    }
                }
            }
            if (bounty > 0) {
                line += " | Bounty " + Format::Thousands(bounty);
            }
            return line;
        }
    }

    std::string Build(std::int64_t a_secondsAway, const Options& a_options)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return {};
        }

        std::vector<std::string> lines;
        if (a_secondsAway >= 0) {
            lines.push_back("Last played " + Format::Ago(a_secondsAway) + " ago");
        }
        if (a_options.showQuest) {
            lines.push_back(QuestLine(player));
        }
        if (a_options.showLocation) {
            lines.push_back(LocationLine(player));
        }
        if (a_options.showSnapshot) {
            lines.push_back(SnapshotLine(player));
        }

        std::string text;
        for (const auto& l : lines) {
            if (!l.empty()) {
                text += (text.empty() ? "" : "\n") + l;
            }
        }
        return text;
    }
}
