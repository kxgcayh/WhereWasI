#include "PCH.h"

#include "Recap.h"

namespace
{
    constexpr std::uint32_t kUniqueID = 'WWAI';
    constexpr std::uint32_t kRecordTime = 'TIME';
    constexpr std::uint32_t kRecordVersion = 1;
    constexpr auto kIniRelPath = "Data/SKSE/Plugins/WhereWasI.ini";
    std::string g_iniPath;  // absolute: GetPrivateProfileString looks in the Windows folder for a relative path

    struct Config
    {
        double minHoursAway{ 4.0 };
        Recap::Options options;
    } g_config;

    std::int64_t g_lastSaved = 0;  // 0 = no record in this save's co-save
    bool g_pendingRecap = false;

    std::string IniString(const char* a_key, const char* a_default)
    {
        char buf[64]{};
        REX::W32::GetPrivateProfileStringA("General", a_key, a_default, buf, sizeof(buf), g_iniPath.c_str());
        return buf;
    }

    void LoadConfig()
    {
        std::error_code ec;
        g_iniPath = (std::filesystem::current_path(ec) / kIniRelPath).string();
        logger::info("ini: '{}' exists={}", g_iniPath, std::filesystem::exists(g_iniPath, ec));
        try {
            g_config.minHoursAway = std::stod(IniString("fMinHoursAway", "4.0"));
        } catch (...) {
            logger::warn("fMinHoursAway is not a number; using 4.0");
        }
        g_config.options.showQuest = IniString("bShowQuest", "1") != "0";
        g_config.options.showLocation = IniString("bShowLocation", "1") != "0";
        g_config.options.showSnapshot = IniString("bShowSnapshot", "1") != "0";
        logger::info("config: fMinHoursAway={} quest={} location={} snapshot={}", g_config.minHoursAway,
            g_config.options.showQuest, g_config.options.showLocation, g_config.options.showSnapshot);
    }

    std::string LocalTime(std::int64_t a_unix)
    {
        std::tm tm{};
        const std::time_t t = a_unix;
        if (localtime_s(&tm, &t) != 0) {
            return "?";
        }
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
        return buf;
    }

    void OnSave(SKSE::SerializationInterface* a_intfc)
    {
        const std::int64_t now = std::time(nullptr);
        if (!a_intfc->WriteRecord(kRecordTime, kRecordVersion, now)) {
            logger::error("co-save: FAILED to write timestamp {}", now);
        } else {
            logger::info("co-save: wrote timestamp {} ({})", now, LocalTime(now));
        }
    }

    void OnLoad(SKSE::SerializationInterface* a_intfc)
    {
        g_lastSaved = 0;
        std::uint32_t type, version, length;
        int records = 0;
        while (a_intfc->GetNextRecordInfo(type, version, length)) {
            ++records;
            if (type == kRecordTime && version == kRecordVersion && length == sizeof(std::int64_t)) {
                if (a_intfc->ReadRecordData(g_lastSaved) != sizeof(std::int64_t)) {
                    logger::error("co-save: short read of timestamp record");
                    g_lastSaved = 0;
                }
            } else {
                logger::warn("co-save: skipped record type {:#x} v{} len {}", type, version, length);
            }
        }
        if (g_lastSaved) {
            logger::info("co-save: {} record(s), timestamp {} ({})", records, g_lastSaved, LocalTime(g_lastSaved));
        } else {
            logger::info("co-save: {} record(s), no timestamp (save made before install, or no co-save)", records);
        }
    }

    void OnRevert(SKSE::SerializationInterface*)
    {
        g_lastSaved = 0;
        logger::info("co-save: revert");
    }

    void ShowRecap()
    {
        const std::int64_t now = std::time(nullptr);
        // -1 = unknown (no stamp). A stamp in the future (clock moved back, other PC) counts as 0, not unknown.
        const std::int64_t away = g_lastSaved ? std::max<std::int64_t>(now - g_lastSaved, 0) : -1;
        const auto threshold = static_cast<std::int64_t>(g_config.minHoursAway * 3600.0);
        logger::info("away: now {} ({}), saved {}, away {}s, threshold {}s", now, LocalTime(now),
            g_lastSaved ? LocalTime(g_lastSaved) : "unknown"s, away, threshold);
        if (away >= 0 && away < threshold) {
            logger::info("under threshold; no recap");
            return;
        }
        const auto text = Recap::Build(away, g_config.options);
        logger::info("recap (away {}s):\n{}", away, text);
        if (text.empty()) {
            logger::warn("recap text is empty; no box shown");
        } else {
            RE::DebugMessageBox(text.c_str());
            logger::info("message box shown");
        }
    }

    // A message box opened during kPostLoadGame sits behind the loading screen, so wait for it to close.
    class LoadingMenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:
        static LoadingMenuSink* GetSingleton()
        {
            static LoadingMenuSink singleton;
            return &singleton;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
            RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
        {
            if (a_event && !a_event->opening && g_pendingRecap &&
                a_event->menuName == RE::LoadingMenu::MENU_NAME) {
                g_pendingRecap = false;
                logger::info("loading menu closed; queueing recap");
                SKSE::GetTaskInterface()->AddTask(ShowRecap);
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
    {
        switch (a_msg->type) {
        case SKSE::MessagingInterface::kDataLoaded:
            if (auto* ui = RE::UI::GetSingleton()) {
                ui->AddEventSink<RE::MenuOpenCloseEvent>(LoadingMenuSink::GetSingleton());
                logger::info("data loaded; menu sink registered");
            } else {
                logger::error("data loaded but RE::UI is null; recap will never show");
            }
            break;
        case SKSE::MessagingInterface::kPreLoadGame:
            logger::info("---- loading save '{}'", a_msg->data ? static_cast<const char*>(a_msg->data) : "");
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
            g_pendingRecap = a_msg->data != nullptr;  // data is the load-success bool
            logger::info("post load game: load {}; pending recap = {}", g_pendingRecap ? "succeeded" : "FAILED", g_pendingRecap);
            break;
        case SKSE::MessagingInterface::kSaveGame:
            logger::info("---- saved game '{}'", a_msg->data ? static_cast<const char*>(a_msg->data) : "");
            break;
        case SKSE::MessagingInterface::kNewGame:
            g_pendingRecap = false;
            logger::info("new game; no recap");
            break;
        default:
            break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    // Rotate on open: a relaunch moves the previous session to WhereWasI.1.log (up to .5) instead of erasing it.
    // InitInfo also sets flush_on(info), so every line reaches disk at once.
    SKSE::Init(a_skse, { .logRotate = 5 });
    logger::info("WhereWasI loading on Skyrim {}", REL::Module::get().version().string());

    LoadConfig();

    auto* serialization = SKSE::GetSerializationInterface();
    serialization->SetUniqueID(kUniqueID);
    serialization->SetSaveCallback(OnSave);
    serialization->SetLoadCallback(OnLoad);
    serialization->SetRevertCallback(OnRevert);

    if (!SKSE::GetMessagingInterface()->RegisterListener(MessageHandler)) {
        logger::error("failed to register messaging listener");
        return false;
    }
    return true;
}
