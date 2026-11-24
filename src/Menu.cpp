#include "Menu.h"
#include "Config.h"

#include "SKSEMenuFramework.h"

namespace
{
	constexpr int kColumns = 3;

	template <class Kind, std::size_t N>
	void Group(const char* a_title, const char* a_help, Kind (&a_kinds)[N])
	{
		if (!ImGuiMCP::CollapsingHeader(a_title, ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
			return;
		}
		bool changed = false;
		ImGuiMCP::PushID(a_title);
		ImGuiMCP::Indent();
		ImGuiMCP::TextColored({ 1.0f, 0.85f, 0.35f, 1.0f }, "%s", a_help);
		const bool allOn = ImGuiMCP::SmallButton("All on");
		ImGuiMCP::SameLine();
		const bool allOff = ImGuiMCP::SmallButton("All off");
		if (allOn || allOff) {
			for (auto& kind : a_kinds) {
				kind.enabled = allOn;
			}
			changed = true;
		}
		if (ImGuiMCP::BeginTable("creatures", kColumns)) {
			for (auto& kind : a_kinds) {
				ImGuiMCP::TableNextColumn();
				changed |= ImGuiMCP::Checkbox(kind.name, &kind.enabled);
			}
			ImGuiMCP::EndTable();
		}
		ImGuiMCP::Unindent();
		ImGuiMCP::PopID();
		ImGuiMCP::Spacing();
		if (changed) {
			Config::SaveSwitches();
		}
	}

	void __stdcall Render()
	{
		auto& data = Config::Get();
		Group("Flight", "The creature flies to a target it can't reach on foot. Spell casters stay in the air above it.", data.flight.kinds);
		Group("Burrowing", "The creature digs into soft ground and bursts out next to its target.", data.burrow.kinds);
	}
}

void Menu::Register()
{
	if (!SKSEMenuFramework::IsInstalled()) {
		logs::info("SKSE Menu Framework not installed - settings are INI-only");
		return;
	}
	SKSEMenuFramework::SetSection("Creature Motion");
	SKSEMenuFramework::AddSectionItem("Creatures", Render);
	logs::info("Registered SKSE Menu Framework page");
}
