#include "Hooks.h"
#include "Papyrus.h"
#include "Settings.h"

namespace {

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
			constexpr auto level = spdlog::level::info;
		#endif

		auto log = std::make_shared<spdlog::logger>("global", std::move(sink));
		log->set_level(level);
		log->flush_on(level);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%s%#]: [%^%L%$] %v"s);
		logger::info("THIS IS A MODIFIED VERSION MADE FOR THE GIANTESS (GTS) MOD.\r\nDO NOT CONTACT ERSHIN IF YOU HAVE ISSUES WITH THIS VERSION");
	}
}


SKSEPluginLoad(const LoadInterface* a_skse) {
	#ifndef NDEBUG
		//while (!IsDebuggerPresent()) {
		//	Sleep(100);
		//}
	#endif
	InitializeLog();

	logger::info("{} v{}", "Dynamic Collision Adjustment GTSMod", Plugin::VERSION.string());

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

SKSEPluginInfo(
	.Version = REL::Version{ 2, 0, 1, 0 },
	.Name = Plugin::NAME,
	.Author = "Ershin, Modified by BingusEx for the GTS Mod",
	.StructCompatibility = SKSE::StructCompatibility::Independent,
	.RuntimeCompatibility = SKSE::VersionIndependence::AddressLibrary
);

