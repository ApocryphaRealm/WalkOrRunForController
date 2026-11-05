// Movement Speed Slider - own code, GPL-3.0-or-later. A slider among the game's own Gameplay settings that sets the player's movement
// speed, kept in the mod's INI and never in the save. See the heads of source/Row.cpp and source/Speed.cpp.
#include "PCH.h"

#include "DevBenchTool.h"
#include "Motion.h"
#include "Row.h"
#include "Settings.h"
#include "Speed.h"
#include "Strings.h"

#include "utils/AddressLibraryGuard.h"
#include "utils/Logger.h"

namespace
{
	void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
	{
		if (!a_msg) { return; }
		switch (a_msg->type)
		{
		case SKSE::MessagingInterface::kPostLoad:
			DevBenchTool::Init(false);
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			msp::Strings::Load();
			speed::Install();
			motion::Install();
			row::Install();
			DevBenchTool::Init(true);
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			// The hook answers from the start, so there is nothing to put back; this only has the game take the speed once more.
			logger::info("game {}: movement speed {}%", a_msg->type == SKSE::MessagingInterface::kNewGame ? "started" : "loaded", settings::SelectedPercent());
			motion::ApplyOnLoad();
			speed::Refresh();
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::log::init("MovementSpeedSlider");
	// Address Library pre-check, BEFORE SKSE::Init, which opens the Address Library itself: a missing file gets a message
	// naming it and the plugin loads inert instead of CommonLibSSE-NG's bare failure line.
	if (!AddressLibraryGuard::Guard("Movement Speed Slider")) { return true; }
	SKSE::Init(a_skse);

	settings::Load();
	settings::ApplyLogLevel();
	SKSE::log::describe_level("MovementSpeedSlider.ini");
	logger::info("Movement Speed Slider {} loading", SKSE::PluginDeclaration::GetSingleton()->GetVersion().string("."));

	auto* messaging = SKSE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(MessageHandler))
	{
		logger::error("could not register the SKSE message listener; the slider will not appear");
		return true;
	}
	return true;
}
