#pragma once

#include "Config.h"
#include "Trace.h"

namespace Legs
{
	struct Report
	{
		int   steps = 0;
		float miss = 0.0f;
		float pace = 0.0f;
	};

	bool   Prepare(RE::Actor* a_actor, const Config::Gait& a_gait);
	void   Walk(RE::Actor* a_actor, const Trace::Probe& a_probe, const RE::NiPoint3& a_point, const RE::NiPoint3& a_normal, const RE::NiPoint3& a_forward, float a_delta);
	void   Rest(RE::Actor* a_actor, float a_delta);
	Report Summary(RE::Actor* a_actor);
	void   Sweep();
	void   Reset();
}
