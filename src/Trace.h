#pragma once

namespace Trace
{
	struct Hit
	{
		RE::NiPoint3 point;
		RE::NiPoint3 normal{ 0, 0, 1 };
		bool         terrain = false;
	};

	class Probe
	{
	public:
		explicit Probe(RE::Actor* a_actor)
		{
			const auto* cell = a_actor ? a_actor->GetParentCell() : nullptr;
			m_world = cell ? cell->GetbhkWorld() : nullptr;
			if (m_world) {
				RE::CFilter own{};
				a_actor->GetCollisionFilterInfo(own);
				m_filter = (own.filter & 0xFFFF0000) | static_cast<std::uint32_t>(RE::COL_LAYER::kCharController);
			}
		}

		explicit operator bool() const { return m_world != nullptr; }

		bool Cast(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, Hit& a_hit) const
		{
			RE::NiPoint3 start = a_from;
			for (int attempt = 0; attempt < kAttempts; ++attempt) {
				const RE::NiPoint3 span = a_to - start;
				const float        length = span.Length();
				if (length < kSkip) {
					return false;
				}
				float        fraction = 0.0f;
				RE::NiPoint3 normal;
				const auto*  body = Pick(start, a_to, fraction, normal);
				if (!body) {
					return false;
				}
				const RE::NiPoint3 point = start + span * fraction;
				if (World(*body)) {
					a_hit.point = point;
					a_hit.normal = normal;
					a_hit.terrain = Terrain(*body);
					return true;
				}
				start = point + span * (kSkip / length);
			}
			return false;
		}

	private:
		static constexpr int   kAttempts = 4;
		static constexpr float kSkip = 2.0f;

		const RE::hkpCollidable* Pick(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, float& a_fraction, RE::NiPoint3& a_normal) const
		{
			const float     scale = RE::bhkWorld::GetWorldScale();
			RE::bhkPickData pick;
			pick.rayInput.from = RE::hkVector4(a_from.x * scale, a_from.y * scale, a_from.z * scale, 0.0f);
			pick.rayInput.to = RE::hkVector4(a_to.x * scale, a_to.y * scale, a_to.z * scale, 0.0f);
			pick.rayInput.filterInfo.filter = m_filter;
			m_world->PickObject(pick);
			if (!pick.rayOutput.HasHit() || !pick.rayOutput.rootCollidable) {
				return nullptr;
			}
			a_fraction = pick.rayOutput.hitFraction;
			a_normal = { pick.rayOutput.normal.quad.m128_f32[0], pick.rayOutput.normal.quad.m128_f32[1], pick.rayOutput.normal.quad.m128_f32[2] };
			return pick.rayOutput.rootCollidable;
		}

		static bool Terrain(const RE::hkpCollidable& a_body)
		{
			const auto layer = a_body.GetCollisionLayer();
			return layer == RE::COL_LAYER::kTerrain || layer == RE::COL_LAYER::kGround;
		}

		static bool World(const RE::hkpCollidable& a_body)
		{
			using Layer = RE::COL_LAYER;
			switch (a_body.GetCollisionLayer()) {
			case Layer::kStatic:
			case Layer::kAnimStatic:
			case Layer::kTransparent:
			case Layer::kTransparentSmall:
			case Layer::kTransparentSmallAnim:
			case Layer::kTrees:
			case Layer::kProps:
			case Layer::kTerrain:
			case Layer::kGround:
			case Layer::kStairHelper:
				break;
			default:
				return false;
			}
			const auto* ref = RE::TESHavokUtilities::FindCollidableRef(a_body);
			return !ref || !ref->Is(RE::FormType::ActorCharacter);
		}

		RE::bhkWorld* m_world = nullptr;
		std::uint32_t m_filter = 0;
	};
}
