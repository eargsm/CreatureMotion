#pragma once

namespace Roster
{
	enum Move
	{
		kFlight,
		kBurrow,
		kCrawl,
		kMoves
	};

	struct Entry
	{
		RE::FormID  race = 0;
		int         kind = -1;
		bool        on = true;
		std::string key;
		std::string label;
	};

	void                Build();
	std::vector<Entry>& Of(Move a_move);
	bool                Allowed(Move a_move, RE::Actor* a_actor);
	void                Save();
}
