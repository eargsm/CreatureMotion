#include "Flight.h"
#include "Config.h"
#include "Creature.h"
#include "Maths.h"
#include "Trace.h"

namespace Flight
{
	using namespace Maths;

	namespace
	{
		constexpr int   kMaxLogs = 400;
		constexpr float kScanEvery = 0.25f;

		struct Kind
		{
			const char* skeleton;
			float       size;
			bool        caster;
			bool        gentle;
		};

		constexpr Kind kKinds[Config::kFlyers] = {
			{ "\\chaurusflyer\\", 1.0f, false, false },
			{ "\\icewraith\\", 1.0f, false, true },
			{ "\\witchlight\\", 0.6f, false, true },
			{ "\\wisp\\", 1.0f, true, true },
			{ "\\netch\\", 2.5f, false, true },
			{ "\\hmdaedra\\", 1.0f, true, true },
			{ "\\atronachflame\\", 1.0f, true, true },
			{ "\\dragonpriest\\", 1.0f, true, true },
			{ "\\atronachstorm\\", 1.2f, true, true },
			{ "\\soulwisp\\", 0.6f, false, true },
			{ "\\vampirelord\\", 1.2f, true, true },
		};

		struct Flyer
		{
			RE::ActorHandle handle, target;
			RE::FormID      id = 0;
			int             kind = 0;
			std::string     name;
			Vec3            pos{}, dir{ 0, 1, 0 }, ground{};
			float           speed = 0, yaw = 0, scale = 1, t = 0, progT = 0, progDist = 1e9f, blocked = 0, logT = 0;
			float           stop = 0, stopT = 0, hoverEnd = 0, sink = 0;
			bool            landing = false, hover = false;
		};

		struct Memo : Creature::Chase
		{
			float lastT = -1e9f;
			Vec3  lastPos{};
		};

		std::vector<Flyer>                    g_flyers;
		std::unordered_map<RE::FormID, float> g_cooldown;
		std::unordered_map<RE::FormID, Memo>  g_memo;
		Creature::Kinds                       g_kinds;
		float                                 g_time = 0, g_scanT = 0;
		int                                   g_logs = 0;

		bool Log()
		{
			if (!Config::Get().flight.log || g_logs >= kMaxLogs) {
				return false;
			}
			++g_logs;
			return true;
		}

		int Classify(RE::Actor* a)
		{
			const auto path = Creature::SkeletonPath(a);
			for (int k = 0; k < Config::kFlyers; ++k) {
				if (path.find(kKinds[k].skeleton) != std::string::npos) {
					return k;
				}
			}
			return -1;
		}

		int KindOf(RE::Actor* a)
		{
			const int kind = g_kinds.Of(a, Classify);
			return kind >= 0 && Config::Get().flight.kinds[kind].enabled ? kind : -1;
		}

		bool Flying(RE::FormID a_id)
		{
			return std::ranges::any_of(g_flyers, [&](const Flyer& f) { return f.id == a_id; });
		}

		Vec3 Turn(const Vec3& a_dir, float a_deg, float a_lift)
		{
			const float a = a_deg * kPi / 180.0f, c = std::cos(a), s = std::sin(a);
			return Unit(Vec3{ a_dir.x * c - a_dir.y * s, a_dir.x * s + a_dir.y * c, a_dir.z + a_lift }, a_dir);
		}

		void End(Flyer& f, RE::Actor* a, const char* a_reason, bool a_air, bool a_good)
		{
			if (a && !a->IsDead()) {
				Creature::Hold(a, a_air ? RE::hkpCharacterStateType::kInAir : RE::hkpCharacterStateType::kOnGround);
			}
			g_cooldown[f.id] = g_time + (a_good ? 1.5f : Config::Get().flight.cooldown);
			if (Log()) {
				logs::info("FLIGHT end {:08X} '{}' {} after {:.1f}s pos=({:.0f},{:.0f},{:.0f})", f.id, f.name, a_reason, f.t, f.pos.x, f.pos.y, f.pos.z);
			}
		}

		void Drive(RE::Actor* a, Flyer& f)
		{
			a->SetPosition(f.pos, true);
			a->SetHeading(f.yaw);
			Creature::Hold(a, RE::hkpCharacterStateType::kOnGround);
			a->SetGraphVariableFloat("Speed", f.speed);
		}

		bool StartLanding(Flyer& f, const Trace::Probe& r, float a_reach, bool a_anySlope)
		{
			Trace::Hit   h;
			const Vec3 from = f.pos + Vec3{ 0, 0, 30.0f * f.scale };
			if (!r.Cast(from, from - Vec3{ 0, 0, a_reach }, h) || (!a_anySlope && h.normal.z < 0.5f)) {
				return false;
			}
			f.landing = true;
			f.ground = h.point;
			return true;
		}

		bool StepLanding(Flyer& f, RE::Actor* a, float dt)
		{
			const auto& cfg = Config::Get().flight;
			const float height = (std::max)(f.pos.z - f.ground.z, 0.0f);
			const bool  gentle = kKinds[f.kind].gentle;
			if (gentle) {
				const float want = std::clamp(height * 1.5f, 25.0f, (std::max)({ cfg.landSpeed, height * 0.5f, 25.0f }));
				f.sink += std::clamp(want - f.sink, -400.0f * dt, 150.0f * dt);
				f.pos.z -= (std::min)(f.sink * dt, height);
				f.speed = (std::max)(f.speed - cfg.accel * dt, 0.0f);
			} else {
				f.pos.z -= (std::min)((std::max)(f.speed, 220.0f) * dt, height);
				f.speed = (std::max)(f.speed - cfg.accel * dt, 120.0f);
			}
			const bool down = f.pos.z - f.ground.z < (gentle ? 2.0f : 6.0f);
			if (down) {
				f.pos = f.ground;
			}
			Drive(a, f);
			if (down) {
				End(f, a, "landed", false, true);
			}
			return !down;
		}

		Vec3 Steer(const Flyer& f, const Trace::Probe& r, const Vec3& a_want, float a_dist, bool& a_free)
		{
			const float sc = f.scale, bodyH = 50.0f * sc, hover = Config::Get().flight.kinds[f.kind].hover * sc;
			const Vec3  c = f.pos + Vec3{ 0, 0, bodyH };
			const float look = (std::max)(140.0f * sc, f.speed * 0.6f);
			Vec3        want = a_want;
			Trace::Hit    h;
			a_free = true;
			if (r.Cast(c, c + want * (std::min)(look, a_dist), h)) {
				a_free = false;
				const Vec3 n = Unit(h.normal, want * -1.0f);
				const Vec3 along = want - n * want.Dot(n);
				const Vec3 options[] = { Unit(along + Vec3{ 0, 0, 0.6f }, Vec3{ 0, 0, 1 }), Turn(want, 45.0f, 0.3f), Turn(want, -45.0f, 0.3f), Vec3{ 0, 0, 1 },
					Turn(want, 90.0f, 0.2f), Turn(want, -90.0f, 0.2f), Unit(along - Vec3{ 0, 0, 0.4f }, Vec3{ 0, 0, -1 }) };
				for (const auto& option : options) {
					if (!r.Cast(c, c + option * (look * 0.7f), h)) {
						want = option;
						a_free = true;
						break;
					}
				}
			}
			if (r.Cast(c, c - Vec3{ 0, 0, bodyH + hover }, h) && c.z - h.point.z < bodyH + hover * 0.6f) {
				want = Unit(Vec3{ want.x, want.y, (std::max)(want.z, 0.35f) }, want);
			}
			if (r.Cast(c, c + Vec3{ 0, 0, 90.0f * sc }, h)) {
				want = Unit(Vec3{ want.x, want.y, (std::min)(want.z, -0.1f) }, want);
			}
			return want;
		}

		float StopDistance(const Trace::Probe& r, const Vec3& a_target, const Vec3& a_toward, float a_want)
		{
			Trace::Hit h;
			for (float d = a_want; d > 170.0f; d *= 0.6f) {
				const Vec3 p = a_target - a_toward * (d * 0.8f) + Vec3{ 0, 0, 60.0f };
				if (r.Cast(p, p - Vec3{ 0, 0, 260.0f }, h) && h.normal.z > 0.5f) {
					return d;
				}
			}
			return (std::min)(a_want, 160.0f);
		}

		bool Step(Flyer& f, float dt)
		{
			const auto& cfg = Config::Get().flight;
			const auto& kc = cfg.kinds[f.kind];
			const auto  held = f.handle.get();
			auto*       a = held.get();
			if (!Creature::Usable(a)) {
				End(f, a, "actor gone", true, false);
				return false;
			}
			const Trace::Probe r(a);
			if (!r) {
				End(f, a, "no physics world", true, false);
				return false;
			}
			f.t += dt;
			if (f.landing) {
				return StepLanding(f, a, dt);
			}

			const auto targetHeld = f.target.get();
			auto*      tgt = targetHeld.get();
			if (!tgt || tgt->IsDead() || !tgt->Is3DLoaded()) {
				if (!StartLanding(f, r, 4000.0f, true)) {
					End(f, a, "target gone", true, false);
					return false;
				}
				return true;
			}
			const float sc = f.scale;
			const Vec3  tpos = tgt->GetPosition();
			const Vec3  toT = tpos - f.pos;
			const float dh = Flat(toT);
			const Vec3  hd = dh > 1.0f ? Vec3{ toT.x / dh, toT.y / dh, 0.0f } : Heading(f.yaw);
			if (f.t >= f.stopT) {
				f.stopT = f.t + 0.5f;
				f.stop = StopDistance(r, tpos, hd, kc.stopDist);
			}
			const Vec3  aim = tpos - hd * (f.stop * 0.8f) + Vec3{ 0, 0, kc.hover * sc };
			const Vec3  d = aim - f.pos;
			const float dist = d.Length();

			if (!f.hover && (dist < 60.0f || (dh < f.stop && std::fabs(d.z) < 100.0f))) {
				f.yaw = Yaw(hd);
				const auto& kind = kKinds[f.kind];
				if (!kind.caster && StartLanding(f, r, 600.0f, false)) {
					Drive(a, f);
					return true;
				}
				if (!kind.caster && !kind.gentle) {
					End(f, a, "arrived (no floor below)", true, true);
					return false;
				}
				f.hover = true;
				f.hoverEnd = f.t + Random((std::max)(cfg.hoverMin, 0.5f), cfg.hoverMax);
				if (Log()) {
					logs::info("FLIGHT {:08X} '{}' hovering for {:.1f}s", f.id, f.name, f.hoverEnd - f.t);
				}
			}
			if (f.hover) {
				if (f.t >= f.hoverEnd) {
					if (!StartLanding(f, r, 4000.0f, true)) {
						End(f, a, "hover over (no floor below)", true, true);
						return false;
					}
					Drive(a, f);
					return true;
				}
			} else if (f.t > cfg.maxSeconds || f.t - f.progT > (std::max)(cfg.stuckSec, 1.0f) + 1.0f || f.blocked > 1.5f) {
				const char* why = f.t > cfg.maxSeconds ? "timeout" : f.blocked > 1.5f ? "blocked" : "no progress";
				if (Log()) {
					logs::info("FLIGHT {:08X} '{}' {} at dist {:.0f} - landing", f.id, f.name, why, dist);
				}
				if (!StartLanding(f, r, 4000.0f, true)) {
					End(f, a, why, true, false);
					return false;
				}
				Drive(a, f);
				return true;
			}
			if (dist < f.progDist - 30.0f) {
				f.progDist = dist;
				f.progT = f.t;
			}

			bool       free = true;
			const Vec3 want = Steer(f, r, dist > 1.0f ? d * (1.0f / dist) : f.dir, dist, free);
			f.dir = Unit(f.dir + (want - f.dir) * (std::min)(1.0f, dt * 5.0f), want);
			const float cruise = f.hover ? Ramp(dist, 25.0f, 300.0f) : 0.35f + 0.65f * Ramp(dist, 40.0f, 260.0f);
			const float speedT = free ? kc.speed * cruise : 0.0f;
			f.speed += std::clamp(speedT - f.speed, -cfg.accel * 2.0f * dt, cfg.accel * dt);

			const Vec3 c = f.pos + Vec3{ 0, 0, 50.0f * sc };
			const Vec3 step = f.dir * (f.speed * dt);
			Trace::Hit   h;
			if (step.Length() > 0.01f && !r.Cast(c, c + step + f.dir * (40.0f * sc), h)) {
				f.pos = f.pos + step;
				f.blocked = (std::max)(0.0f, f.blocked - dt);
			} else if (!f.hover) {
				f.speed *= 0.5f;
				f.blocked += dt;
			}
			if (f.hover) {
				f.yaw = Yaw(hd);
			} else if (Flat(f.dir) > 0.3f) {
				f.yaw = Yaw(f.dir);
			}
			Drive(a, f);
			if (f.t - f.logT > 2.0f && Log()) {
				f.logT = f.t;
				logs::info("FLIGHT {:08X} '{}' t={:.1f} dist={:.0f} speed={:.0f} dir=({:.2f},{:.2f},{:.2f}) blocked={:.2f}", f.id, f.name, f.t, dist, f.speed, f.dir.x, f.dir.y,
					f.dir.z, f.blocked);
			}
			return true;
		}

		void Begin(RE::Actor* a, RE::Actor* a_target, int a_kind, const char* a_why)
		{
			Flyer f;
			f.handle = a->GetHandle();
			f.target = a_target->GetHandle();
			f.id = a->GetFormID();
			f.kind = a_kind;
			f.name = a->GetName() ? a->GetName() : Config::Get().flight.kinds[a_kind].name;
			f.pos = a->GetPosition();
			f.yaw = a->data.angle.z;
			f.dir = Unit(Heading(f.yaw) + Vec3{ 0, 0, 0.3f }, Vec3{ 0, 0, 1 });
			f.scale = Creature::Scale(a) * kKinds[a_kind].size;
			if (Log()) {
				const Vec3 d = a_target->GetPosition() - f.pos;
				logs::info("FLIGHT start {:08X} '{}' ({}) {} dz={:.0f} dist={:.0f}", f.id, f.name, Config::Get().flight.kinds[a_kind].name, a_why, d.z, d.Length());
			}
			g_flyers.push_back(std::move(f));
		}

		bool Sees(const Trace::Probe& r, RE::Actor* a, RE::Actor* a_target)
		{
			Trace::Hit   h;
			const Vec3 up{ 0, 0, 70.0f };
			return !r.Cast(a->GetPosition() + up, a_target->GetPosition() + up, h);
		}

		bool AtLedge(const Trace::Probe& r, RE::Actor* a, const Vec3& a_move)
		{
			const float drop = Config::Get().flight.ledgeDrop;
			const Vec3  chest = a->GetPosition() + Vec3{ 0, 0, 60.0f };
			Trace::Hit    h;
			const auto* controller = a->GetCharController();
			if (controller && controller->context.currentState == RE::hkpCharacterStateType::kInAir) {
				return !r.Cast(chest, chest - Vec3{ 0, 0, 60.0f + drop }, h);
			}
			const float speed = Flat(a_move);
			if (speed < 40.0f) {
				return false;
			}
			const Vec3 ahead = chest + Vec3{ a_move.x / speed, a_move.y / speed, 0.0f } * (std::max)(110.0f, speed * 0.4f);
			return !r.Cast(chest, ahead, h) && !r.Cast(ahead, ahead - Vec3{ 0, 0, 60.0f + drop }, h);
		}

		const char* Trigger(RE::Actor* a, RE::Actor* a_target, int a_kind, float a_now)
		{
			const auto& cfg = Config::Get().flight;
			const auto& kc = cfg.kinds[a_kind];
			const Vec3  pos = a->GetPosition();
			const Vec3  d = a_target->GetPosition() - pos;
			const float distance = d.Length();
			auto&       memo = g_memo[a->GetFormID()];
			memo.Note(a_target->GetFormID(), distance, a_now);
			const float since = a_now - memo.lastT;
			const Vec3  move = since > 0.05f && since < 1.0f ? (pos - memo.lastPos) * (1.0f / since) : Vec3{};
			memo.lastPos = pos;
			memo.lastT = a_now;
			if (distance > cfg.engage) {
				return nullptr;
			}
			const Trace::Probe r(a);
			if (!r) {
				return nullptr;
			}
			const char* why = nullptr;
			if (kKinds[a_kind].gentle && cfg.ledge && AtLedge(r, a, move)) {
				why = "ledge";
			} else if (distance >= kc.stopDist * 1.5f) {
				const bool above = d.z >= cfg.minRise;
				const bool stalled = memo.Stalled(a_now, cfg.stuckSec) && Flat(d) > kc.stopDist * 1.5f;
				if ((above || stalled) && !(kKinds[a_kind].caster && Sees(r, a, a_target))) {
					why = above ? "target above" : "no progress";
				}
			}
			if (why) {
				memo.closest = distance;
				memo.closestAt = a_now;
			}
			return why;
		}

		void Scan(float a_now)
		{
			const auto& cfg = Config::Get().flight;
			if (std::ranges::none_of(cfg.kinds, &Config::Flyer::enabled)) {
				return;
			}
			Creature::EachFighter(cfg.engage, [&](RE::Actor* a, RE::Actor* a_target) {
				if (static_cast<int>(g_flyers.size()) >= cfg.maxFlyers) {
					return false;
				}
				const int kind = KindOf(a);
				if (kind < 0 || Flying(a->GetFormID())) {
					return true;
				}
				if (const auto it = g_cooldown.find(a->GetFormID()); it != g_cooldown.end() && a_now < it->second) {
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
		g_flyers.clear();
		g_cooldown.clear();
		g_memo.clear();
		g_kinds.Clear();
		g_logs = 0;
	}

	void Update(float a_delta)
	{
		g_time += a_delta;
		std::erase_if(g_flyers, [&](Flyer& f) { return !Step(f, a_delta); });
		if (g_time - g_scanT > kScanEvery) {
			g_scanT = g_time;
			Scan(g_time);
		}
	}
}
