#include "Hooks.h"
#include "Papyrus.h"
#include "Settings.h"

void MessageHandler(SKSE::MessagingInterface::Message* a_msg) {
	switch (a_msg->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			Settings::Initialize();
			Settings::ReadSettings();
			Settings::RequestAPIs();
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			Settings::OnPostLoadGame();
			break;
	}
}

namespace {
	void InitializeLog() {
		#ifndef NDEBUG
			auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
		#else
			auto path = logger::log_directory();
			if (!path) {
				util::report_and_fail("Failed to find standard logging directory"sv);
			}

			*path /= fmt::format("{}.log", Plugin::NAME);
			auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
		#endif

		#ifndef NDEBUG
			const auto level = spdlog::level::trace;
		#else
			const auto level = spdlog::level::info;
		#endif

		auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
		log->set_level(level);
		log->flush_on(level);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%s%#]: [%^%L%$] %v"s);
		logger::info("THIS IS A MODIFIED VERSION MADE FOR THE GIANTESS MOD.\r\nI SWEAR TO GOD IF YOU GO BOTHER ERSHIN WHILE USING THIS VERSION I WILL PERSONALLY COME TO YOUR HOUSE TO SLAP YOU.");
	}
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info) {
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Plugin::NAME.data();
	a_info->version = Plugin::VERSION[0];

	if (a_skse->IsEditor()) {
		//logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_SSE_1_5_39) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;

	v.PluginVersion(Plugin::VERSION);
	v.PluginName(Plugin::NAME);
	v.AuthorName("Ershin, Modified by Arial for the GTS Mod");
	v.UsesAddressLibrary(true);
	v.CompatibleVersions({ SKSE::RUNTIME_SSE_LATEST });
	v.HasNoStructUse(true);
	return v;
}();

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse){
	#ifndef NDEBUG
		//while (!IsDebuggerPresent()) {
		//	Sleep(100);
		//}
	#endif
	InitializeLog();

	logger::info("{} v{}", "Dynamic Collision Adjustment GTSMOD", Plugin::VERSION.string());

	SKSE::Init(a_skse);
	SKSE::AllocTrampoline(1 << 8);

	auto messaging = SKSE::GetMessagingInterface();
	if (!messaging->RegisterListener("SKSE", MessageHandler)) {
		return false;
	}

	Hooks::Install();
	Papyrus::Register();

	return true;
}
