#include "Crawl.h"
#include "Burrow.h"
#include "Config.h"
#include "Creature.h"
#include "Maths.h"
#include "Trace.h"

namespace Crawl
{
	using namespace Maths;

	namespace
	{
		constexpr int   kMaxLogs = 400;
		constexpr float kScanEvery = 0.25f;
		constexpr char  kSkeletonRoot[] = "NPC Root [Root]";

		struct Kind
		{
			const char* skeleton;
			float       size;
		};

		constexpr Kind kKinds[Config::kCrawlers] = {
			{ "\\chaurus\\", 1.0f },
			{ "\\frostbitespider\\", 1.0f },
			{ "\\dwarvenspider\\", 0.8f },
			{ "\\scrib\\", 0.5f },
		};

		struct Crawler
		{
			RE::ActorHandle               handle, target;
			RE::FormID                    id = 0;
			int                           kind = 0;
			std::string                   name;
			Vec3                          point{}, normal{ 0, 0, 1 }, forward{ 0, 1, 0 };
			float                         scale = 1, speed = 0, yaw = 0, t = 0, progT = 0, progDist = 1e9f, logT = 0, lost = 0;
			bool                          climbed = false;
			RE::NiPointer<RE::NiAVObject> body;
			RE::NiTransform               rest;
		};

		std::vector<Crawler>                             g_crawlers;
		std::unordered_map<RE::FormID, float>            g_cooldown;
		std::unordered_map<RE::FormID, Creature::Chase>  g_memo;
		Creature::Kinds                                  g_kinds;
		float                                            g_time = 0, g_scanT = 0;
		int                                              g_logs = 0, g_skipLogs = 0;

		bool Log()
		{
			if (!Config::Get().crawl.log || g_logs >= kMaxLogs) {
				return false;
			}
			++g_logs;
			return true;
		}

		int Classify(RE::Actor* a)
		{
			const auto path = Creature::SkeletonPath(a);
			for (int k = 0; k < Config::kCrawlers; ++k) {
				if (path.find(kKinds[k].skeleton) != std::string::npos) {
					return k;
				}
			}
			return -1;
		}

		int KindOf(RE::Actor* a)
		{
			const int kind = g_kinds.Of(a, Classify);
			return kind >= 0 && Config::Get().crawl.kinds[kind].enabled ? kind : -1;
		}

		Vec3 Along(const Vec3& a_v, const Vec3& a_normal) { return a_v - a_normal * a_v.Dot(a_normal); }

		RE::NiAVObject* TiltNode(RE::Actor* a)
		{
			auto* root = a->Get3D(false);
			auto* bones = root ? root->GetObjectByName(RE::BSFixedString(kSkeletonRoot)) : nullptr;
			auto* above = bones ? bones->parent : nullptr;
			return above && above != root ? above : nullptr;
		}

		void Pose(RE::Actor* a, Crawler& c)
		{
			const float standOff = Config::Get().crawl.standOff * c.scale;
			a->SetPosition(c.point + c.normal * standOff, true);
			if (Flat(c.forward) > 0.3f) {
				c.yaw = Yaw(c.forward);
			}
			a->SetHeading(c.yaw);
			Creature::Hold(a, RE::hkpCharacterStateType::kOnGround);
			a->SetGraphVariableFloat("Speed", c.speed);

			const auto* root = a->Get3D(false);
			if (!root || !c.body) {
				return;
			}
			const Vec3      right = Unit(c.forward.Cross(c.normal), Vec3{ 1, 0, 0 });
			RE::NiMatrix3   world;
			for (int row = 0; row < 3; ++row) {
				world.entry[row][0] = right[row];
				world.entry[row][1] = c.forward[row];
				world.entry[row][2] = c.normal[row];
			}
			const auto toLocal = root->world.rotate.Transpose();
			c.body->local.rotate = toLocal * world;
			c.body->local.translate = c.rest.translate + (toLocal * (c.normal * -standOff)) * (1.0f / (std::max)(root->world.scale, 0.05f));
		}

		void End(Crawler& c, RE::Actor* a, const char* a_reason, bool a_good)
		{
			if (c.body) {
				c.body->local = c.rest;
			}
			if (a && !a->IsDead()) {
				const bool standing = c.normal.z > 0.5f;
				if (standing) {
					a->SetPosition(c.point, true);
				}
				Creature::Hold(a, standing ? RE::hkpCharacterStateType::kOnGround : RE::hkpCharacterStateType::kInAir);
			}
			g_cooldown[c.id] = g_time + (a_good ? 1.5f : Config::Get().crawl.cooldown);
			Creature::rests.Start(c.id, Creature::kRestAfterMove);
			if (Log()) {
				logs::info("CRAWL end {:08X} '{}' {} after {:.1f}s normal=({:.2f},{:.2f},{:.2f}) pos=({:.0f},{:.0f},{:.0f})", c.id, c.name, a_reason, c.t, c.normal.x, c.normal.y,
					c.normal.z, c.point.x, c.point.y, c.point.z);
			}
		}

		bool Advance(Crawler& c, const Trace::Probe& r, float a_step)
		{
			const float  lift = 40.0f * c.scale, reach = 30.0f * c.scale;
			const Vec3   over = c.point + c.normal * lift;
			const Vec3   next = c.point + c.forward * a_step;
			Trace::Hit   hit;
			const bool   found = r.Cast(over, over + c.forward * (a_step + reach), hit) || r.Cast(next + c.normal * lift, next - c.normal * lift, hit) ||
			                     r.Cast(next - c.normal * reach + c.forward * lift, next - c.normal * reach - c.forward * lift, hit);
			if (!found) {
				return false;
			}
			c.point = hit.point;
			c.normal = Unit(c.normal + (Unit(hit.normal, c.normal) - c.normal) * 0.35f, hit.normal);
			return true;
		}

		bool Step(Crawler& c, float dt)
		{
			const auto& cfg = Config::Get().crawl;
			const auto  held = c.handle.get();
			auto*       a = held.get();
			if (!Creature::Usable(a) || a->Get3D(false) == nullptr || TiltNode(a) != c.body.get()) {
				End(c, a, "actor gone", false);
				return false;
			}
			const Trace::Probe r(a);
			const auto         targetHeld = c.target.get();
			auto*              tgt = targetHeld.get();
			if (!r || !tgt || tgt->IsDead() || !tgt->Is3DLoaded()) {
				End(c, a, "target gone", false);
				return false;
			}
			c.t += dt;
			const Vec3  goal = tgt->GetPosition() + Vec3{ 0, 0, 40.0f };
			const Vec3  toGoal = goal - c.point;
			const float dist = toGoal.Length();
			c.climbed = c.climbed || c.normal.z < 0.5f;
			const bool  level = c.climbed && c.normal.z > 0.8f && std::fabs(toGoal.z) < 90.0f;
			if (dist < cfg.stopDist || level) {
				End(c, a, level ? "reached its level" : "arrived", true);
				return false;
			}
			if (dist < c.progDist - 30.0f) {
				c.progDist = dist;
				c.progT = c.t;
			}
			if (c.t > cfg.maxSeconds || c.t - c.progT > cfg.stuckSec + 2.0f || c.lost > 0.5f) {
				End(c, a, c.lost > 0.5f ? "lost the surface" : c.t > cfg.maxSeconds ? "timeout" : "no progress", false);
				return false;
			}

			const Vec3 want = Unit(Along(toGoal, c.normal), c.forward);
			c.forward = Unit(Along(c.forward + (want - c.forward) * (std::min)(1.0f, dt * 4.0f), c.normal), want);
			c.speed += std::clamp(cfg.speed - c.speed, -cfg.accel * dt, cfg.accel * dt);
			if (Advance(c, r, c.speed * dt)) {
				c.lost = 0.0f;
				c.forward = Unit(Along(c.forward, c.normal), want);
			} else {
				c.lost += dt;
				c.speed *= 0.5f;
			}
			Pose(a, c);
			if (c.t - c.logT > 1.0f && Log()) {
				c.logT = c.t;
				logs::info("CRAWL {:08X} '{}' t={:.1f} dist={:.0f} speed={:.0f} normal=({:.2f},{:.2f},{:.2f}) forward=({:.2f},{:.2f},{:.2f}) lost={:.2f}", c.id, c.name, c.t, dist,
					c.speed, c.normal.x, c.normal.y, c.normal.z, c.forward.x, c.forward.y, c.forward.z, c.lost);
			}
			return true;
		}

		bool FindFloor(const Trace::Probe& r, RE::Actor* a, Trace::Hit& a_floor)
		{
			const Vec3 feet = a->GetPosition();
			return r.Cast(feet + Vec3{ 0, 0, 50.0f }, feet - Vec3{ 0, 0, 100.0f }, a_floor);
		}

		void Begin(RE::Actor* a, RE::Actor* a_target, int a_kind, const Trace::Hit& a_floor, const char* a_why)
		{
			Crawler c;
			c.handle = a->GetHandle();
			c.target = a_target->GetHandle();
			c.id = a->GetFormID();
			c.kind = a_kind;
			c.name = a->GetName() ? a->GetName() : Config::Get().crawl.kinds[a_kind].name;
			c.scale = Creature::Scale(a) * kKinds[a_kind].size;
			c.point = a_floor.point;
			c.normal = Unit(a_floor.normal, Vec3{ 0, 0, 1 });
			c.yaw = a->data.angle.z;
			c.forward = Unit(Along(a_target->GetPosition() - c.point, c.normal), Heading(c.yaw));
			c.body.reset(TiltNode(a));
			c.rest = c.body->local;
			if (Log()) {
				const Vec3 d = a_target->GetPosition() - a->GetPosition();
				logs::info("CRAWL start {:08X} '{}' ({}) {} dz={:.0f} dist={:.0f} tilt node='{}' floor normal=({:.2f},{:.2f},{:.2f})", c.id, c.name,
					Config::Get().crawl.kinds[a_kind].name, a_why, d.z, d.Length(), c.body->name.c_str(), c.normal.x, c.normal.y, c.normal.z);
			}
			g_crawlers.push_back(std::move(c));
		}

		void Consider(RE::Actor* a, RE::Actor* a_target, int a_kind, float a_now)
		{
			const auto& cfg = Config::Get().crawl;
			const Vec3  d = a_target->GetPosition() - a->GetPosition();
			const float distance = d.Length();
			auto&       memo = g_memo[a->GetFormID()];
			memo.Note(a_target->GetFormID(), a->GetPosition(), distance, a_now);
			if (distance > cfg.engage || distance < cfg.stopDist * 1.5f) {
				return;
			}
			const bool above = d.z >= cfg.minRise;
			if (!above && !memo.Stuck(a_now, cfg.stuckSec)) {
				return;
			}
			const Trace::Probe r(a);
			Trace::Hit         floor;
			const bool         node = TiltNode(a) != nullptr;
			if (!r || !node || !FindFloor(r, a, floor)) {
				if (cfg.log && g_skipLogs++ < 40) {
					logs::info("CRAWL skip {:08X} '{}' {}", a->GetFormID(), a->GetName(), node ? "no floor under it" : "skeleton has no node to tilt");
				}
				g_cooldown[a->GetFormID()] = a_now + 2.0f;
				return;
			}
			memo.closest = distance;
			memo.closestAt = a_now;
			Begin(a, a_target, a_kind, floor, above ? "target above" : "stuck");
		}

		void Scan(float a_now)
		{
			const auto& cfg = Config::Get().crawl;
			if (std::ranges::none_of(cfg.kinds, &Config::Crawler::enabled)) {
				return;
			}
			Creature::EachFighter(cfg.engage, [&](RE::Actor* a, RE::Actor* a_target) {
				if (static_cast<int>(g_crawlers.size()) >= cfg.maxCrawlers) {
					return false;
				}
				const int kind = KindOf(a);
				if (kind < 0 || Busy(a->GetFormID()) || Burrow::Busy(a->GetFormID())) {
					return true;
				}
				if (const auto it = g_cooldown.find(a->GetFormID()); it != g_cooldown.end() && a_now < it->second) {
					return true;
				}
				Consider(a, a_target, kind, a_now);
				return true;
			});
		}
	}

	bool Busy(RE::FormID a_id)
	{
		return std::ranges::any_of(g_crawlers, [&](const Crawler& c) { return c.id == a_id; });
	}

	void Reset()
	{
		for (auto& c : g_crawlers) {
			const auto held = c.handle.get();
			End(c, held.get(), "reset", false);
		}
		g_crawlers.clear();
		g_cooldown.clear();
		g_memo.clear();
		g_kinds.Clear();
		g_logs = 0;
		g_skipLogs = 0;
	}

	void Update(float a_delta)
	{
		g_time += a_delta;
		std::erase_if(g_crawlers, [&](Crawler& c) { return !Step(c, a_delta); });
		if (g_time - g_scanT > kScanEvery) {
			g_scanT = g_time;
			Scan(g_time);
		}
	}
}
