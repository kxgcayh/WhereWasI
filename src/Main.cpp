#include "PCH.h"

#include "Recap.h"

namespace
{
    constexpr std::uint32_t kUniqueID = 'WWAI';
    constexpr std::uint32_t kRecordTime = 'TIME';
    constexpr std::uint32_t kRecordVersion = 1;
    constexpr auto kIniPath = "Data/SKSE/Plugins/WhereWasI.ini";

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
        REX::W32::GetPrivateProfileStringA("General", a_key, a_default, buf, sizeof(buf), kIniPath);
        return buf;
    }

    void LoadConfig()
    {
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

    void OnSave(SKSE::SerializationInterface* a_intfc)
    {
        const std::int64_t now = std::time(nullptr);
        if (!a_intfc->WriteRecord(kRecordTime, kRecordVersion, now)) {
            logger::error("failed to write save timestamp");
        }
    }

    void OnLoad(SKSE::SerializationInterface* a_intfc)
    {
        std::uint32_t type, version, length;
        while (a_intfc->GetNextRecordInfo(type, version, length)) {
            if (type == kRecordTime && version == kRecordVersion && length == sizeof(std::int64_t)) {
                a_intfc->ReadRecordData(g_lastSaved);
            }
        }
    }

    void OnRevert(SKSE::SerializationInterface*) { g_lastSaved = 0; }

    void ShowRecap()
    {
        const std::int64_t away = g_lastSaved ? std::time(nullptr) - g_lastSaved : -1;
        if (away >= 0 && away < static_cast<std::int64_t>(g_config.minHoursAway * 3600.0)) {
            logger::info("away {}s, under threshold; no recap", away);
            return;
        }
        const auto text = Recap::Build(away, g_config.options);
        logger::info("recap (away {}s):\n{}", away, text);
        if (!text.empty()) {
            RE::DebugMessageBox(text.c_str());
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
            }
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
            g_pendingRecap = a_msg->data != nullptr;  // data is the load-success bool
            break;
        case SKSE::MessagingInterface::kNewGame:
            g_pendingRecap = false;
            break;
        default:
            break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);  // opens Documents/My Games/.../SKSE/WhereWasI.log
    logger::info("WhereWasI loading");

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
