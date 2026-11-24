# Creature Motion

SKSE plugin for Skyrim SE / AE that changes how creatures reach you.

- Flight: Chaurus Hunter, Ice Wraith, Wisp, Wispmother, Netch, Seeker, Flame Atronach, Storm Atronach, Dragon Priest, Soul Cairn Wisp, Vampire Lord.
- Burrowing: Mudcrab, Ash Hopper, Chaurus, Chaurus Reaper, Ash Spawn.
- Wall crawling with procedural legs: Chaurus, Frostbite Spider, Dwarven Spider, Ash Hopper.

Creatures from other mods are picked up on their own when they use one of these skeletons or behaviour files.

It needs SKSE and Address Library. SKSE Menu Framework is optional and adds an in-game page with a switch for every creature, including the ones found in your load order.

Settings: `SKSE/Plugins/CreatureMotion.ini`.

Build: `git submodule update --init --recursive`, then `xmake f -p windows -m releasedbg -y` and `xmake -y`.

Licensed under GPL-3.0 with the modding exception in `EXCEPTIONS`. Built with CommonLibSSE-NG; the SKSE Menu Framework header in `extern/` is under its own license.
