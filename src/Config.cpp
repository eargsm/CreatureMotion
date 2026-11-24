#include "Config.h"
#include "Ini.h"

namespace
{
	const std::filesystem::path kPath{ "Data/SKSE/Plugins/CreatureMotion.ini" };
	Config::Data                g_data;

	void ReadFlight(const Ini::File& a_ini, Config::Flight& a_flight)
	{
		const char* s = "Flight";
		a_flight.maxFlyers = std::clamp(a_ini.Int(s, "iMaxFlyers", a_flight.maxFlyers), 1, 12);
		a_flight.always = a_ini.Bool(s, "bAlwaysFly", a_flight.always);
		a_flight.accel = a_ini.Float(s, "fAccel", a_flight.accel);
		a_flight.engage = a_ini.Float(s, "fEngage", a_flight.engage);
		a_flight.minRise = a_ini.Float(s, "fMinRise", a_flight.minRise);
		a_flight.stuckSec = a_ini.Float(s, "fStuckSec", a_flight.stuckSec);
		a_flight.maxSeconds = a_ini.Float(s, "fMaxSeconds", a_flight.maxSeconds);
		a_flight.cooldown = a_ini.Float(s, "fCooldown", a_flight.cooldown);
		a_flight.hoverMin = a_ini.Float(s, "fHoverSecMin", a_flight.hoverMin);
		a_flight.hoverMax = a_ini.Float(s, "fHoverSecMax", a_flight.hoverMax);
		a_flight.landSpeed = a_ini.Float(s, "fLandSpeed", a_flight.landSpeed);
		a_flight.ledge = a_ini.Bool(s, "bLedge", a_flight.ledge);
		a_flight.ledgeDrop = a_ini.Float(s, "fLedgeDrop", a_flight.ledgeDrop);
		a_flight.rangedChance = std::clamp(a_ini.Float(s, "fRangedChance", a_flight.rangedChance), 0.0f, 1.0f);
		a_flight.rangedDist = a_ini.Float(s, "fRangedDist", a_flight.rangedDist);
		a_flight.log = a_ini.Bool(s, "bLog", a_flight.log);
		for (auto& kind : a_flight.kinds) {
			kind.enabled = a_ini.Bool(kind.section, "bFlight", kind.enabled);
			kind.speed = a_ini.Float(kind.section, "fSpeed", kind.speed);
			kind.stopDist = a_ini.Float(kind.section, "fStopDist", kind.stopDist);
			kind.hover = a_ini.Float(kind.section, "fHover", kind.hover);
		}
	}

	void ReadBurrow(const Ini::File& a_ini, Config::Burrow& a_burrow)
	{
		const char* s = "Burrow";
		a_burrow.maxBurrowers = std::clamp(a_ini.Int(s, "iMaxBurrowers", a_burrow.maxBurrowers), 1, 8);
		a_burrow.speed = a_ini.Float(s, "fSpeed", a_burrow.speed);
		a_burrow.engage = a_ini.Float(s, "fEngage", a_burrow.engage);
		a_burrow.minDist = a_ini.Float(s, "fMinDist", a_burrow.minDist);
		a_burrow.farDist = a_ini.Float(s, "fFarDist", a_burrow.farDist);
		a_burrow.nearChance = a_ini.Float(s, "fNearChance", a_burrow.nearChance);
		a_burrow.farChance = a_ini.Float(s, "fFarChance", a_burrow.farChance);
		a_burrow.maxRise = a_ini.Float(s, "fMaxRise", a_burrow.maxRise);
		a_burrow.stuckSec = a_ini.Float(s, "fStuckSec", a_burrow.stuckSec);
		a_burrow.cooldown = a_ini.Float(s, "fCooldown", a_burrow.cooldown);
		a_burrow.retrySec = a_ini.Float(s, "fRetrySec", a_burrow.retrySec);
		a_burrow.digSec = a_ini.Float(s, "fDigSec", a_burrow.digSec);
		a_burrow.riseSec = a_ini.Float(s, "fRiseSec", a_burrow.riseSec);
		a_burrow.depth = a_ini.Float(s, "fDepth", a_burrow.depth);
		a_burrow.exitDist = a_ini.Float(s, "fExitDist", a_burrow.exitDist);
		a_burrow.maxSeconds = a_ini.Float(s, "fMaxSeconds", a_burrow.maxSeconds);
		a_burrow.burstScale = a_ini.Float(s, "fBurstScale", a_burrow.burstScale);
		a_burrow.rocks = a_ini.Bool(s, "bChaurusRocks", a_burrow.rocks);
		a_burrow.stagger = a_ini.Bool(s, "bStagger", a_burrow.stagger);
		a_burrow.staggerDist = a_ini.Float(s, "fStaggerDist", a_burrow.staggerDist);
		a_burrow.shake = a_ini.Bool(s, "bCameraShake", a_burrow.shake);
		a_burrow.log = a_ini.Bool(s, "bLog", a_burrow.log);
		for (auto& kind : a_burrow.kinds) {
			kind.enabled = a_ini.Bool(kind.section, "bBurrow", kind.enabled);
			kind.chance = std::clamp(a_ini.Float(kind.section, "fBurrowChance", kind.chance), 0.0f, 1.0f);
		}
	}

	void ReadCrawl(const Ini::File& a_ini, Config::Crawl& a_crawl)
	{
		const char* s = "Crawl";
		a_crawl.maxCrawlers = std::clamp(a_ini.Int(s, "iMaxCrawlers", a_crawl.maxCrawlers), 1, 12);
		a_crawl.speed = a_ini.Float(s, "fSpeed", a_crawl.speed);
		a_crawl.accel = a_ini.Float(s, "fAccel", a_crawl.accel);
		a_crawl.engage = a_ini.Float(s, "fEngage", a_crawl.engage);
		a_crawl.minRise = a_ini.Float(s, "fMinRise", a_crawl.minRise);
		a_crawl.stuckSec = a_ini.Float(s, "fStuckSec", a_crawl.stuckSec);
		a_crawl.stopDist = a_ini.Float(s, "fStopDist", a_crawl.stopDist);
		a_crawl.standOff = a_ini.Float(s, "fStandOff", a_crawl.standOff);
		a_crawl.maxSeconds = a_ini.Float(s, "fMaxSeconds", a_crawl.maxSeconds);
		a_crawl.cooldown = a_ini.Float(s, "fCooldown", a_crawl.cooldown);
		a_crawl.proceduralLegs = a_ini.Bool(s, "bProceduralLegs", a_crawl.proceduralLegs);
		a_crawl.footReach = a_ini.Float(s, "fFootReach", a_crawl.footReach);
		a_crawl.log = a_ini.Bool(s, "bLog", a_crawl.log);
		for (auto& kind : a_crawl.kinds) {
			kind.enabled = a_ini.Bool(kind.section, "bCrawl", kind.enabled);
			kind.gait.stepLength = std::clamp(a_ini.Float(kind.section, "fStepLength", kind.gait.stepLength), 0.05f, 1.5f);
			kind.gait.stepHeight = std::clamp(a_ini.Float(kind.section, "fStepHeight", kind.gait.stepHeight), 0.0f, 1.0f);
			kind.gait.stance = std::clamp(a_ini.Float(kind.section, "fStance", kind.gait.stance), 0.3f, 0.95f);
			kind.gait.swing = std::clamp(a_ini.Float(kind.section, "fSwing", kind.gait.swing), 0.15f, 0.48f);
			kind.gait.arch = std::clamp(a_ini.Float(kind.section, "fArch", kind.gait.arch), 0.0f, 1.0f);
			kind.gait.fan = std::clamp(a_ini.Float(kind.section, "fFan", kind.gait.fan), 0.0f, 1.0f);
		}
	}
}

Config::Data& Config::Get() { return g_data; }

const std::filesystem::path& Config::Path() { return kPath; }

void Config::Load()
{
	const Ini::File ini{ kPath };
	if (!ini.Found()) {
		logs::warn("{} not found, using defaults", kPath.string());
	}
	Data data;
	ReadFlight(ini, data.flight);
	ReadBurrow(ini, data.burrow);
	ReadCrawl(ini, data.crawl);
	g_data = data;
	const auto flying = std::ranges::count_if(g_data.flight.kinds, &Flyer::enabled);
	const auto digging = std::ranges::count_if(g_data.burrow.kinds, &Burrower::enabled);
	const auto crawling = std::ranges::count_if(g_data.crawl.kinds, &Crawler::enabled);
	logs::info("Settings loaded: flight on for {} of {} creatures, burrowing on for {} of {}, wall crawling on for {} of {}", flying, kFlyers, digging, kBurrowers, crawling,
		kCrawlers);
}

void Config::SaveSwitches()
{
	Ini::File ini{ kPath };
	for (const auto& kind : g_data.flight.kinds) {
		ini.Set(kind.section, "bFlight", kind.enabled);
	}
	for (const auto& kind : g_data.burrow.kinds) {
		ini.Set(kind.section, "bBurrow", kind.enabled);
	}
	for (const auto& kind : g_data.crawl.kinds) {
		ini.Set(kind.section, "bCrawl", kind.enabled);
	}
	if (!ini.Write()) {
		logs::error("Could not write {}", kPath.string());
	}
}
