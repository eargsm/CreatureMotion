#pragma once

namespace Config
{
	inline constexpr int kFlyers = 11;
	inline constexpr int kBurrowers = 5;
	inline constexpr int kCrawlers = 4;

	struct Flyer
	{
		const char* name;
		const char* section;
		bool        enabled;
		float       speed;
		float       stopDist;
		float       hover;
	};

	struct Flight
	{
		int   maxFlyers = 8;
		bool  always = true;
		float accel = 700.0f;
		float engage = 3000.0f;
		float minRise = 120.0f;
		float stuckSec = 2.0f;
		float maxSeconds = 25.0f;
		float cooldown = 4.0f;
		float hoverMin = 6.0f;
		float hoverMax = 14.0f;
		float landSpeed = 110.0f;
		bool  ledge = true;
		float ledgeDrop = 180.0f;
		float rangedChance = 0.75f;
		float rangedDist = 450.0f;
		bool  log = false;
		Flyer kinds[kFlyers]{
			{ "Chaurus Hunter", "Hunter", true, 420.0f, 160.0f, 80.0f },
			{ "Ice Wraith", "IceWraith", true, 380.0f, 140.0f, 60.0f },
			{ "Wisp", "Wisp", true, 300.0f, 120.0f, 60.0f },
			{ "Wispmother", "Wispmother", true, 300.0f, 500.0f, 150.0f },
			{ "Netch", "Netch", true, 220.0f, 250.0f, 30.0f },
			{ "Seeker", "Seeker", true, 280.0f, 500.0f, 150.0f },
			{ "Flame Atronach", "FlameAtronach", true, 320.0f, 500.0f, 150.0f },
			{ "Dragon Priest", "DragonPriest", true, 260.0f, 600.0f, 150.0f },
			{ "Storm Atronach", "StormAtronach", true, 300.0f, 500.0f, 150.0f },
			{ "Soul Cairn Wisp", "SoulWisp", true, 300.0f, 120.0f, 60.0f },
			{ "Vampire Lord", "VampireLord", true, 320.0f, 400.0f, 150.0f },
		};
	};

	struct Burrower
	{
		const char* name;
		const char* section;
		bool        enabled;
		float       chance;
	};

	struct Burrow
	{
		int      maxBurrowers = 2;
		float    speed = 500.0f;
		float    engage = 2500.0f;
		float    minDist = 450.0f;
		float    farDist = 1500.0f;
		float    nearChance = 0.2f;
		float    farChance = 2.0f;
		float    maxRise = 250.0f;
		float    stuckSec = 2.0f;
		float    cooldown = 12.0f;
		float    retrySec = 8.0f;
		float    digSec = 0.9f;
		float    riseSec = 0.5f;
		float    depth = 140.0f;
		float    exitDist = 90.0f;
		float    maxSeconds = 10.0f;
		float    burstScale = 1.0f;
		bool     rocks = true;
		bool     stagger = true;
		float    staggerDist = 110.0f;
		bool     shake = true;
		bool     log = false;
		Burrower kinds[kBurrowers]{
			{ "Mudcrab", "Mudcrab", true, 1.0f },
			{ "Skeever", "Skeever", true, 1.0f },
			{ "Ash Hopper", "AshHopper", true, 1.0f },
			{ "Chaurus", "Chaurus", true, 0.25f },
			{ "Ash Spawn", "AshSpawn", true, 1.0f },
		};
	};

	struct Crawler
	{
		const char* name;
		const char* section;
		bool        enabled;
	};

	struct Crawl
	{
		int     maxCrawlers = 4;
		float   speed = 230.0f;
		float   accel = 600.0f;
		float   engage = 2500.0f;
		float   minRise = 120.0f;
		float   stuckSec = 2.0f;
		float   stopDist = 120.0f;
		float   standOff = 30.0f;
		float   maxSeconds = 25.0f;
		float   cooldown = 4.0f;
		bool    log = false;
		Crawler kinds[kCrawlers]{
			{ "Chaurus", "Chaurus", true },
			{ "Frostbite Spider", "FrostbiteSpider", true },
			{ "Dwarven Spider", "DwarvenSpider", true },
			{ "Ash Hopper", "AshHopper", true },
		};
	};

	struct Data
	{
		Flight flight;
		Burrow burrow;
		Crawl  crawl;
	};

	Data& Get();
	void  Load();
	void  SaveSwitches();
}
