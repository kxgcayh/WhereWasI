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
            const char* questName = quest->GetFullName();
            const auto objective = Format::ResolveTags(found->displayText.c_str(),
                [quest](std::string_view a) { return AliasName(quest, a); });
            logger::info("quest source: {:#x} '{}' name '{}' objective {} text '{}' resolved '{}'", quest->GetFormID(),
                quest->GetFormEditorID(), questName ? questName : "", found->index, found->displayText.c_str(),
                objective ? *objective : "<unresolved alias, dropped>"s);

            std::string line;
            if (questName && *questName) {
                line = questName;
            }
            // An objective with an unfillable alias would read "Bring the  to X": show the quest name alone.
            if (objective && !objective->empty()) {
                line += (line.empty() ? "" : " - ") + *objective;
            }
            return line.empty() ? line : "Quest: " + line;
        }

        // First non-empty name on a location and its parents (a wilderness area location is often unnamed; its hold is not).
        const char* LocationName(const RE::BGSLocation* a_loc)
        {
            for (int depth = 0; a_loc && depth < 8; a_loc = a_loc->parentLoc, ++depth) {
                const char* n = a_loc->GetFullName();
                logger::info("location chain [{}]: {:#x} '{}'", depth, a_loc->GetFormID(), n ? n : "");
                if (n && *n) {
                    return n;
                }
            }
            return nullptr;
        }

        // Wilderness cells often carry no location at all (current=0, cellLoc=0). The world map's answer is the nearest
        // map marker: they are persistent refs in the worldspace's persistent cell.
        std::string NearestMarker(RE::PlayerCharacter* a_player, const RE::TESWorldSpace* a_ws)
        {
            if (!a_ws || !a_ws->persistentCell) {
                return {};
            }
            // Two exterior cells (4096 units each). Farther than that, "near X" misleads; fall back to the worldspace.
            constexpr float kMaxDist = 8192.0f;
            const auto pos = a_player->GetPosition();
            std::string best;
            float bestDist = kMaxDist * kMaxDist;
            int seen = 0;
            a_ws->persistentCell->ForEachReference([&](RE::TESObjectREFR* a_ref) {
                const auto* marker = a_ref ? a_ref->extraList.GetByType<RE::ExtraMapMarker>() : nullptr;
                const auto* data = marker ? marker->mapData : nullptr;
                // Visible = discovered (or shown on the map): an undiscovered place's name would be a spoiler.
                const char* n = data && data->flags.all(RE::MapMarkerData::Flag::kVisible) ? data->locationName.fullName.c_str() : nullptr;
                if (n && *n) {
                    ++seen;
                    const auto d = a_ref->GetPosition() - pos;
                    if (const float dist = d.x * d.x + d.y * d.y; dist < bestDist) {
                        bestDist = dist;
                        best = n;
                    }
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
            logger::info("nearest map marker: '{}' at {:.0f} units, {} visible markers, limit {:.0f} (player at {:.0f},{:.0f})",
                best, best.empty() ? 0.0f : std::sqrt(bestDist), seen, kMaxDist, pos.x, pos.y);
            return best;
        }

        std::string LocationLine(RE::PlayerCharacter* a_player)
        {
            const auto* cell = a_player->GetParentCell();
            const auto* ws = a_player->GetWorldspace();
            const auto* cur = a_player->GetCurrentLocation();
            const auto* cellLoc = cell ? cell->GetLocation() : nullptr;
            logger::info("location sources: current={:#x} cellLoc={:#x} cell='{}' worldspace='{}'",
                cur ? cur->GetFormID() : 0, cellLoc ? cellLoc->GetFormID() : 0,
                cell && cell->GetFullName() ? cell->GetFullName() : "", ws && ws->GetFullName() ? ws->GetFullName() : "");

            const char* name = LocationName(cur);
            if (!name || !*name) {
                name = LocationName(cellLoc);
            }
            if ((!name || !*name) && cell) {
                name = cell->GetFullName();
            }
            if (!name || !*name) {
                if (auto marker = NearestMarker(a_player, ws); !marker.empty()) {
                    return "Location: near " + marker;
                }
            }
            // Last resort: the worldspace ("Skyrim", "Solstheim") still tells where you are.
            if ((!name || !*name) && ws) {
                name = ws->GetFullName();
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
