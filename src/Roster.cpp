#include "Roster.h"
#include "Burrow.h"
#include "Config.h"
#include "Crawl.h"
#include "Creature.h"
#include "Flight.h"
#include "Ini.h"

namespace Roster
{
	namespace
	{
		constexpr const char* kSections[kMoves] = { "FlightCreatures", "BurrowCreatures", "CrawlCreatures" };

		std::vector<Entry>             g_entries[kMoves];
		std::unordered_set<RE::FormID> g_off[kMoves];

		void Refresh()
		{
			for (int move = 0; move < kMoves; ++move) {
				g_off[move].clear();
				for (const auto& entry : g_entries[move]) {
					if (!entry.on) {
						g_off[move].insert(entry.race);
					}
				}
			}
		}

		template <class Sort>
		int KindOf(const RE::TESRace* a_race, Sort&& a_sort)
		{
			const int kind = a_sort(Creature::TrailOf(a_race, RE::SEX::kMale));
			return kind >= 0 ? kind : a_sort(Creature::TrailOf(a_race, RE::SEX::kFemale));
		}

		bool Listed(const Ini::File& a_ini, const char* a_section, const char* a_key)
		{
			return a_ini.Bool(a_section, a_key, true) == a_ini.Bool(a_section, a_key, false);
		}
	}

	void Build()
	{
		for (auto& entries : g_entries) {
			entries.clear();
		}
		auto* data = RE::TESDataHandler::GetSingleton();
		if (!data) {
			return;
		}
		const Ini::File ini{ Config::Path() };
		for (const auto* race : data->GetFormArray<RE::TESRace>()) {
			if (!race) {
				continue;
			}
			const int kinds[kMoves] = { KindOf(race, Flight::Sort), KindOf(race, Burrow::Sort), KindOf(race, Crawl::Sort) };
			if (std::ranges::all_of(kinds, [](int a_kind) { return a_kind < 0; })) {
				continue;
			}
			const char*       id = race->GetFormEditorID();
			const char*       name = race->GetFullName();
			const auto*       file = race->GetFile(0);
			const std::string key = id && *id ? id : std::format("{:08X}", race->GetFormID());
			const std::string label = std::format("{} ({})  [{}]", name && *name ? name : "Unnamed", key, file ? file->GetFilename() : "?");
			for (int move = 0; move < kMoves; ++move) {
				if (kinds[move] >= 0) {
					g_entries[move].push_back({ race->GetFormID(), kinds[move], ini.Bool(kSections[move], key.c_str(), true), key, label });
				}
			}
		}
		for (auto& entries : g_entries) {
			std::ranges::sort(entries, [](const Entry& a_one, const Entry& a_other) { return std::tie(a_one.kind, a_one.label) < std::tie(a_other.kind, a_other.label); });
		}
		Refresh();
		logs::info("Creatures found in the load order: {} fly, {} burrow, {} crawl", g_entries[kFlight].size(), g_entries[kBurrow].size(), g_entries[kCrawl].size());
	}

	std::vector<Entry>& Of(Move a_move) { return g_entries[a_move]; }

	bool Allowed(Move a_move, RE::Actor* a_actor)
	{
		const auto* race = a_actor->GetRace();
		return !race || !g_off[a_move].contains(race->GetFormID());
	}

	void Save()
	{
		Ini::File ini{ Config::Path() };
		for (int move = 0; move < kMoves; ++move) {
			for (const auto& entry : g_entries[move]) {
				if (!entry.on || Listed(ini, kSections[move], entry.key.c_str())) {
					ini.Set(kSections[move], entry.key.c_str(), entry.on);
				}
			}
		}
		if (!ini.Write()) {
			logs::error("Could not write {}", Config::Path().string());
		}
		Refresh();
	}
}
