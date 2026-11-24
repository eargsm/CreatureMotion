#include "Frame.h"
#include "Burrow.h"
#include "Crawl.h"
#include "Flight.h"

namespace
{
	constexpr float kLongestStep = 0.5f;

	struct PlayerUpdate
	{
		static void thunk(RE::PlayerCharacter* a_player, float a_delta)
		{
			func(a_player, a_delta);
			auto* ui = RE::UI::GetSingleton();
			if (a_delta <= 0.0f || a_delta > kLongestStep || (ui && ui->GameIsPaused())) {
				return;
			}
			Flight::Update(a_delta);
			Burrow::Update(a_delta);
			Crawl::Update(a_delta);
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};
}

void Frame::Install()
{
	REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE_PlayerCharacter[0] };
	PlayerUpdate::func = vtable.write_vfunc(REL::Module::IsVR() ? 0xAF : 0xAD, PlayerUpdate::thunk);
	logs::info("Frame update installed");
}
