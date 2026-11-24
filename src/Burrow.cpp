#include "Burrow.h"
#include "Config.h"
#include "Creature.h"
#include "Maths.h"
#include "Trace.h"

namespace Burrow
{
	using namespace Maths;

	namespace
	{
		constexpr int        kMaxLogs = 400;
		constexpr float      kScanEvery = 0.25f;
		constexpr RE::FormID kDigInSound = 0x000E670F;
		constexpr RE::FormID kDigOutSound = 0x000E670E;
		constexpr RE::FormID kRockExplosion = 0x000E7722;

		struct Dust
		{
			const char* burst;
			const char* trail;
			float       trailScale;
		};

		constexpr Dust kDirtDust{ "Effects\\ImpactEffects\\FXDragonTailStompDust.nif", "Effects\\ImpactEffects\\ImpactArrowDirt01.nif", 2.5f };
		constexpr Dust kSnowDust{ "Effects\\ImpactEffects\\FXDragonTailStompSnow.nif", "Effects\\ImpactEffects\\FXDragonTailStompSnow.nif", 0.12f };
		constexpr Dust kGrassDust{ "Effects\\ImpactEffects\\FXDragonTailStompDustGrass.nif", "Effects\\ImpactEffects\\FXDragonTailStompDustGrass.nif", 0.12f };
		constexpr Dust kAshDust{ "DLC02\\Effects\\DLC2AshSpawnExp01.nif", "Effects\\ImpactEffects\\ImpactArrowDust01.nif", 2.5f };

		struct Kind
		{
			const char* skeleton;
			const char* race;
			float       size;
			bool        rocks;
		};

		constexpr Kind kKinds[Config::kBurrowers] = {
			{ "\\mudcrab\\", nullptr, 0.8f, false },
			{ "\\skeever\\", nullptr, 0.7f, false },
			{ "\\scrib\\", nullptr, 0.6f, false },
			{ "\\chaurus\\", nullptr, 1.3f, true },
			{ nullptr, "ashspawn", 1.4f, false },
		};

		enum class Phase
		{
			kDig,
			kTravel,
			kRise
		};

		struct Digger
		{
			RE::ActorHandle handle, target;
			RE::FormID      id = 0;
			int             kind = 0;
			std::string     name;
			Phase           phase = Phase::kDig;
			Vec3            surface{}, exit{};
			float           depth = 0, sunk = 0, yaw = 0, t = 0, phaseT = 0, exitT = 0, trailT = 0, angle = 0;
			bool            hidden = false;
			RE::MATERIAL_ID ground = RE::MATERIAL_ID::kNone;
		};

		struct Memo : Creature::Chase
		{
			float declined = -1e9f;
		};

		std::vector<Digger>                   g_diggers;
		std::unordered_map<RE::FormID, float> g_cooldown;
		std::unordered_map<RE::FormID, Memo>  g_memo;
		Creature::Kinds                       g_kinds;
		float                                 g_time = 0, g_scanT = 0;
		int                                   g_logs = 0, g_groundLogs = 0;

		bool Log()
		{
			if (!Config::Get().burrow.log || g_logs >= kMaxLogs) {
				return false;
			}
			++g_logs;
			return true;
		}

		int Classify(RE::Actor* a)
		{
			const auto path = Creature::SkeletonPath(a);
			const auto race = Creature::RaceID(a);
			for (int k = 0; k < Config::kBurrowers; ++k) {
				const auto& kind = kKinds[k];
				if ((kind.skeleton && path.find(kind.skeleton) != std::string::npos) || (kind.race && race.find(kind.race) != std::string::npos)) {
					return k;
				}
			}
			return -1;
		}

		int KindOf(RE::Actor* a)
		{
			const int kind = g_kinds.Of(a, Classify);
			return kind >= 0 && Config::Get().burrow.kinds[kind].enabled ? kind : -1;
		}

		bool Digging(RE::FormID a_id)
		{
			return std::ranges::any_of(g_diggers, [&](const Digger& d) { return d.id == a_id; });
		}

		RE::MATERIAL_ID GroundAt(const Vec3& a_pos, RE::MATERIAL_ID a_fallback)
		{
			auto*      tes = RE::TES::GetSingleton();
			const auto material = tes ? tes->GetLandMaterialType(a_pos) : RE::MATERIAL_ID::kNone;
			return material == RE::MATERIAL_ID::kNone ? a_fallback : material;
		}

		RE::MATERIAL_ID GroundUnder(RE::Actor* a)
		{
			const auto* controller = a->GetCharController();
			const auto  material = controller ? controller->surfaceMaterial : RE::MATERIAL_ID::kNone;
			return material == RE::MATERIAL_ID::kNone ? GroundAt(a->GetPosition(), material) : material;
		}

		bool Soft(RE::MATERIAL_ID a_material)
		{
			using M = RE::MATERIAL_ID;
			switch (a_material) {
			case M::kDirt:
			case M::kSand:
			case M::kSnow:
			case M::kGrass:
			case M::kMud:
			case M::kGravel:
			case M::kAsh:
				return true;
			default:
				return false;
			}
		}

		const Dust& DustFor(RE::MATERIAL_ID a_material)
		{
			using M = RE::MATERIAL_ID;
			switch (a_material) {
			case M::kSnow:
			case M::kIce:
				return kSnowDust;
			case M::kGrass:
				return kGrassDust;
			case M::kAsh:
				return kAshDust;
			default:
				return kDirtDust;
			}
		}

		void Effect(RE::Actor* a, const char* a_model, const Vec3& a_pos, float a_scale, float a_life)
		{
			if (auto* cell = a->GetParentCell()) {
				RE::BSTempEffectParticle::Spawn(cell, a_life, a_model, RE::NiPoint3{}, a_pos, a_scale, 7, nullptr);
			}
		}

		void Rocks(RE::Actor* a, RE::Actor* a_target, const Vec3& a_pos)
		{
			auto* explosion = RE::TESForm::LookupByID<RE::BGSExplosion>(kRockExplosion);
			if (!explosion) {
				return;
			}
			if (auto ref = (a_target ? a_target : a)->PlaceObjectAtMe(explosion, false)) {
				ref->SetPosition(a_pos + Vec3{ 0, 0, 15.0f });
			}
		}

		void Sound(RE::FormID a_sound, const Vec3& a_pos)
		{
			auto* descriptor = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(a_sound);
			auto* audio = RE::BSAudioManager::GetSingleton();
			if (!descriptor || !audio) {
				return;
			}
			RE::BSSoundHandle handle;
			if (audio->GetSoundHandle(handle, descriptor)) {
				handle.SetPosition(a_pos);
				handle.Play();
			}
		}

		void Hide(Digger& d, RE::Actor* a, bool a_hide)
		{
			if (d.hidden == a_hide) {
				return;
			}
			if (auto* root = a ? a->Get3D(false) : nullptr) {
				root->SetAppCulled(a_hide);
			}
			d.hidden = a_hide;
		}

		void Drive(RE::Actor* a, Digger& d)
		{
			a->SetPosition(d.surface - Vec3{ 0, 0, d.sunk }, true);
			a->SetHeading(d.yaw);
			Creature::Hold(a, RE::hkpCharacterStateType::kOnGround);
		}

		void End(Digger& d, RE::Actor* a, const char* a_reason)
		{
			if (a) {
				Hide(d, a, false);
				if (!a->IsDead()) {
					d.sunk = 0.0f;
					Drive(a, d);
				}
			}
			g_cooldown[d.id] = g_time + Config::Get().burrow.cooldown;
			if (Log()) {
				logs::info("BURROW end {:08X} '{}' {} after {:.1f}s pos=({:.0f},{:.0f},{:.0f})", d.id, d.name, a_reason, d.t, d.surface.x, d.surface.y, d.surface.z);
			}
		}

		bool Floor(const Trace::Probe& r, const Vec3& a_at, float a_up, float a_down, Vec3& a_out)
		{
			Trace::Hit h;
			if (r.Cast(a_at + Vec3{ 0, 0, a_up }, a_at - Vec3{ 0, 0, a_down }, h) && h.normal.z > 0.4f) {
				a_out = h.point;
				return true;
			}
			return false;
		}

		void PickExit(Digger& d, const Trace::Probe& r, RE::Actor* a_target)
		{
			const auto& cfg = Config::Get().burrow;
			const Vec3  tpos = a_target->GetPosition();
			const float base = Yaw(d.surface - tpos) + d.angle;
			for (const float dist : { cfg.exitDist, cfg.exitDist * 0.5f }) {
				if (Floor(r, tpos + Heading(base) * dist, 80.0f, 200.0f, d.exit)) {
					return;
				}
			}
			d.exit = tpos;
		}

		void Burst(Digger& d, RE::Actor* a, RE::Actor* a_target)
		{
			const auto& cfg = Config::Get().burrow;
			const float size = kKinds[d.kind].size;
			const auto  under = a_target && a_target->GetCharController() ? GroundUnder(a_target) : RE::MATERIAL_ID::kNone;
			const auto& dust = DustFor(under == RE::MATERIAL_ID::kNone ? GroundAt(d.surface, d.ground) : under);
			Effect(a, dust.burst, d.surface, 0.35f * size * cfg.burstScale, 3.0f);
			if (kKinds[d.kind].rocks && cfg.rocks) {
				Rocks(a, a_target, d.surface);
			}
			Sound(kDigOutSound, d.surface);
			const auto* player = RE::PlayerCharacter::GetSingleton();
			if (cfg.shake && player && (player->GetPosition() - d.surface).Length() < 450.0f) {
				RE::ShakeCamera(0.6f * size, d.surface, 0.35f);
			}
			if (cfg.stagger && a_target && !a_target->IsDead() && (a_target->GetPosition() - d.surface).Length() < cfg.staggerDist) {
				a_target->SetGraphVariableFloat("staggerDirection", 0.5f);
				a_target->SetGraphVariableFloat("staggerMagnitude", 0.25f);
				a_target->NotifyAnimationGraph("staggerStart");
			}
		}

		void Travel(Digger& d, RE::Actor* a, RE::Actor* a_target, const Trace::Probe& r, float dt)
		{
			const auto& cfg = Config::Get().burrow;
			if (a_target && d.t >= d.exitT) {
				d.exitT = d.t + 0.3f;
				PickExit(d, r, a_target);
			}
			const Vec3  to = d.exit - d.surface;
			const float left = Flat(to);
			const float move = cfg.speed * dt;
			if (left > 1.0f) {
				const float f = (std::min)(move / left, 1.0f);
				d.surface.x += to.x * f;
				d.surface.y += to.y * f;
				d.yaw = Yaw(to);
			}
			Vec3 ground;
			if (Floor(r, d.surface, 150.0f, 350.0f, ground)) {
				d.surface.z = ground.z;
			}
			Drive(a, d);
			if (d.t - d.trailT > 0.18f) {
				d.trailT = d.t;
				const auto& dust = DustFor(GroundAt(d.surface, d.ground));
				Effect(a, dust.trail, d.surface, dust.trailScale * kKinds[d.kind].size * cfg.burstScale, 1.5f);
			}
			const bool arrived = left <= move + 5.0f;
			if (a_target && !arrived && d.phaseT <= cfg.maxSeconds) {
				return;
			}
			if (a_target) {
				if (arrived) {
					d.surface = d.exit;
				}
				d.yaw = Yaw(a_target->GetPosition() - d.surface);
			}
			Hide(d, a, false);
			Burst(d, a, a_target);
			d.phase = Phase::kRise;
			d.phaseT = 0.0f;
			if (Log()) {
				logs::info("BURROW {:08X} '{}' surfacing at ({:.0f},{:.0f},{:.0f}) after {:.1f}s underground", d.id, d.name, d.surface.x, d.surface.y, d.surface.z, d.t);
			}
		}

		bool Step(Digger& d, float dt)
		{
			const auto& cfg = Config::Get().burrow;
			const auto  held = d.handle.get();
			auto*       a = held.get();
			if (!Creature::Usable(a)) {
				End(d, a, "actor gone");
				return false;
			}
			const Trace::Probe r(a);
			if (!r) {
				End(d, a, "no physics world");
				return false;
			}
			d.t += dt;
			d.phaseT += dt;

			switch (d.phase) {
			case Phase::kDig:
				{
					const float k = std::clamp(d.phaseT / (std::max)(cfg.digSec, 0.1f), 0.0f, 1.0f);
					d.sunk = d.depth * k * k;
					Drive(a, d);
					if (k >= 1.0f) {
						Hide(d, a, true);
						d.phase = Phase::kTravel;
						d.phaseT = 0.0f;
					}
					return true;
				}
			case Phase::kTravel:
				{
					const auto targetHeld = d.target.get();
					auto*      tgt = targetHeld.get();
					Travel(d, a, tgt && !tgt->IsDead() && tgt->Is3DLoaded() ? tgt : nullptr, r, dt);
					return true;
				}
			default:
				{
					const float k = std::clamp(d.phaseT / (std::max)(cfg.riseSec, 0.1f), 0.0f, 1.0f);
					d.sunk = d.depth * (1.0f - k) * (1.0f - k);
					Drive(a, d);
					if (k >= 1.0f) {
						End(d, a, "surfaced");
						return false;
					}
					return true;
				}
			}
		}

		void Begin(RE::Actor* a, RE::Actor* a_target, int a_kind, const char* a_why)
		{
			const auto& cfg = Config::Get().burrow;
			Digger      d;
			d.handle = a->GetHandle();
			d.target = a_target->GetHandle();
			d.id = a->GetFormID();
			d.kind = a_kind;
			d.name = a->GetName() ? a->GetName() : cfg.kinds[a_kind].name;
			d.surface = a->GetPosition();
			d.exit = a_target->GetPosition();
			d.yaw = a->data.angle.z;
			d.angle = Random(-1.2f, 1.2f);
			d.depth = cfg.depth * kKinds[a_kind].size * Creature::Scale(a);
			d.ground = GroundUnder(a);
			Effect(a, DustFor(d.ground).burst, d.surface, 0.25f * kKinds[a_kind].size * cfg.burstScale, 3.0f);
			Sound(kDigInSound, d.surface);
			if (Log()) {
				logs::info("BURROW start {:08X} '{}' ({}) {} dist={:.0f} ground={}", d.id, d.name, cfg.kinds[a_kind].name, a_why, (d.exit - d.surface).Length(),
					RE::MaterialIDToString(d.ground));
			}
			g_diggers.push_back(std::move(d));
		}

		const char* Trigger(RE::Actor* a, RE::Actor* a_target, int a_kind, float a_now)
		{
			const auto& cfg = Config::Get().burrow;
			const Vec3  d = a_target->GetPosition() - a->GetPosition();
			const float distance = d.Length(), flat = Flat(d);
			auto&       memo = g_memo[a->GetFormID()];
			memo.Note(a_target->GetFormID(), distance, a_now);
			if (a_now < memo.declined || distance > cfg.engage || flat < cfg.minDist || std::fabs(d.z) > cfg.maxRise) {
				return nullptr;
			}
			const auto here = GroundUnder(a), there = GroundUnder(a_target);
			if (!Soft(here) || !Soft(there)) {
				memo.declined = a_now + 3.0f;
				if (cfg.log && g_groundLogs++ < 40) {
					logs::info("BURROW skip {:08X} '{}' hard ground: here={} target={}", a->GetFormID(), a->GetName(), RE::MaterialIDToString(here), RE::MaterialIDToString(there));
				}
				return nullptr;
			}
			const bool  stalled = memo.Stalled(a_now, cfg.stuckSec);
			const float reach = stalled ? 1.0f : std::clamp((flat - cfg.minDist) / (std::max)(cfg.farDist - cfg.minDist, 1.0f), 0.0f, 1.0f);
			const float chance = cfg.kinds[a_kind].chance * (cfg.nearChance + (cfg.farChance - cfg.nearChance) * reach);
			if (Random(0.0f, 1.0f) > chance) {
				memo.declined = a_now + cfg.retrySec;
				return nullptr;
			}
			memo.closest = distance;
			memo.closestAt = a_now;
			return stalled ? "no progress" : "chase";
		}

		void Scan(float a_now)
		{
			const auto& cfg = Config::Get().burrow;
			if (std::ranges::none_of(cfg.kinds, &Config::Burrower::enabled)) {
				return;
			}
			Creature::EachFighter(cfg.engage, [&](RE::Actor* a, RE::Actor* a_target) {
				if (static_cast<int>(g_diggers.size()) >= cfg.maxBurrowers) {
					return false;
				}
				const int kind = KindOf(a);
				if (kind < 0 || Digging(a->GetFormID())) {
					return true;
				}
				if (const auto it = g_cooldown.find(a->GetFormID()); it != g_cooldown.end() && a_now < it->second) {
					return true;
				}
				const auto* state = a->AsActorState();
				if (!state || state->IsSwimming() || state->IsBleedingOut() || state->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal) {
					return true;
				}
				if (const char* why = Trigger(a, a_target, kind, a_now)) {
					Begin(a, a_target, kind, why);
				}
				return true;
			});
		}
	}

	void Reset()
	{
		for (auto& d : g_diggers) {
			const auto held = d.handle.get();
			End(d, held.get(), "reset");
		}
		g_diggers.clear();
		g_cooldown.clear();
		g_memo.clear();
		g_kinds.Clear();
		g_logs = 0;
		g_groundLogs = 0;
	}

	void Update(float a_delta)
	{
		g_time += a_delta;
		std::erase_if(g_diggers, [&](Digger& d) { return !Step(d, a_delta); });
		if (g_time - g_scanT > kScanEvery) {
			g_scanT = g_time;
			Scan(g_time);
		}
	}
}
