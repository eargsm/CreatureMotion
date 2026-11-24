#include "Burrow.h"
#include "Config.h"
#include "Crawl.h"
#include "Creature.h"
#include "Flight.h"
#include "Frame.h"
#include "Menu.h"
#include "Roster.h"

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);

	logs::info("Creature Motion loading");

	SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* a_msg) {
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kPostLoad:
			Menu::Register();
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			Config::Load();
			Roster::Build();
			Frame::Install();
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			Config::Load();
			Flight::Reset();
			Burrow::Reset();
			Crawl::Reset();
			Creature::rests.Clear();
			break;
		default:
			break;
		}
	});

	return true;
}
