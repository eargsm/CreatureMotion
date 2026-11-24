#pragma once

#include "Creature.h"

namespace Burrow
{
	void Update(float a_delta);
	void Reset();
	bool Busy(RE::FormID a_id);
	int  Sort(const Creature::Trail& a_trail);
}
