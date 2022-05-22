#include "Events.h"
#include "Hooks.h"
#include "Serialization.h"
#include "Settings.h"

void MessageHandler(SKSE::MessagingInterface::Message* a_message)
{
	switch (a_message->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		{
			Events::Manager::Register();
			Events::AnimationManager::Register();

			Serialization::ClearUnreferencedSaveData();
		}
		break;
	case SKSE::MessagingInterface::kSaveGame:
		{
			std::string savePath = { static_cast<char*>(a_message->data), a_message->dataLen };
			Serialization::Save(savePath);
		}
		break;
	case SKSE::MessagingInterface::kPreLoadGame:
		{
			std::string savePath({ static_cast<char*>(a_message->data), a_message->dataLen });
			string::replace_last_instance(savePath, ".ess", "");
			Serialization::Load(savePath);
		}
		break;
	case SKSE::MessagingInterface::kDeleteGame:
		{
			std::string savePath({ static_cast<char*>(a_message->data), a_message->dataLen });
			Serialization::Delete(savePath);
		}
		break;
	default:
		break;
	}
}

#ifdef SKYRIM_AE
extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;
	v.PluginVersion(Version::MAJOR);
	v.PluginName("Equipment Toggle");
	v.AuthorName("powerofthree");
	v.UsesAddressLibrary(true);
	v.CompatibleVersions({ SKSE::RUNTIME_LATEST });

	return v;
}();
#else
extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = "Equipment Toggle";
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_1_5_39) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}
#endif

void InitializeLog()
{
	auto path = logger::log_directory();
	if (!path) {
		stl::report_and_fail("Failed to find standard logging directory"sv);
	}

	*path /= fmt::format(FMT_STRING("{}.log"), Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

	log->set_level(spdlog::level::debug);
	log->flush_on(spdlog::level::debug);

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%l] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	InitializeLog();

	logger::info("loaded");

	SKSE::Init(a_skse);

	SKSE::AllocTrampoline(112);

	Settings::GetSingleton()->LoadSettings();

	Hooks::Install();

    const auto messaging = SKSE::GetMessagingInterface();
	messaging->RegisterListener(MessageHandler);

	return true;
}
