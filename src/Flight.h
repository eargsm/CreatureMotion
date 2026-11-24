#pragma once

#include "Creature.h"

namespace Flight
{
	void Update(float a_delta);
	void Reset();
	int  Sort(const Creature::Trail& a_trail);
}
