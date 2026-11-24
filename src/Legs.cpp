#include "Legs.h"
#include "Config.h"
#include "Maths.h"

namespace Legs
{
	using namespace Maths;

	namespace
	{
		constexpr char        kSkeletonRoot[] = "NPC Root [Root]";
		constexpr char        kHelperName[] = "CreatureMotionLeg";
		constexpr std::size_t kFewestBones = 3;
		constexpr float       kTwinTolerance = 0.15f;
		constexpr float       kShortFraction = 0.8f;
		constexpr float       kNarrowest = 0.3f;
		constexpr float       kBlendSeconds = 0.25f;
		constexpr float       kFollow = 10.0f;
		constexpr float       kSlowest = 40.0f;
		constexpr float       kStill = 10.0f;
		constexpr float       kFastest = 3.0f;
		constexpr float       kSettle = 1.5f;
		constexpr float       kSteady = 0.5f;

		struct Leg
		{
			std::vector<RE::NiNode*>  chain;
			RE::NiPointer<RE::NiNode> hip, knee;
			std::size_t               bend = 1;
			float                     length = 0.0f, reach = 0.0f;
			bool                      right = false;
			int                       group = 0;
			Vec3                      home{}, socket{}, side{};
			Vec3                      planted{}, from{}, to{}, goal{}, last{}, drift{};
			bool                      stepping = false, paired = false;
			float                     span = 0.0f, limit = 0.0f, rise = 0.0f;
		};

		struct Rig
		{
			RE::NiAVObject*  root = nullptr;
			std::vector<Leg> legs;
			Config::Gait     gait{};
			Vec3             up{ 0, 0, 1 };
			float            blend = 0.0f, miss = 0.0f, phase = 0.0f, stride = 0.0f, pace = 0.0f, span = 0.0f, limit = 0.0f, rise = 0.0f;
			int              worst = -1;
			int              steps = 0;
			bool             fresh = true, touched = false;
		};

		struct Pose
		{
			RE::NiMatrix3 turn;
			Vec3          at;
			float         scale = 1.0f;
		};

		struct Frame
		{
			Vec3  point, normal, forward, right;
			float scale = 1.0f;
		};

		std::unordered_map<RE::FormID, Rig> g_rigs;

		std::vector<RE::NiNode*> Bones(RE::NiNode* a_node)
		{
			std::vector<RE::NiNode*> bones;
			for (auto& child : a_node->GetChildren()) {
				if (auto* node = child ? child->AsNode() : nullptr) {
					bones.push_back(node);
				}
			}
			return bones;
		}

		void Leaves(RE::NiNode* a_node, std::vector<RE::NiNode*>& a_out)
		{
			const auto below = Bones(a_node);
			if (below.empty()) {
				a_out.push_back(a_node);
			}
			for (auto* node : below) {
				Leaves(node, a_out);
			}
		}

		Leg Limb(RE::NiNode* a_tip, const RE::NiNode* a_skeleton)
		{
			Leg leg;
			leg.chain.push_back(a_tip);
			for (auto* top = a_tip; top->parent && top->parent != a_skeleton;) {
				if (Bones(top->parent).size() > 1 && leg.chain.size() >= kFewestBones) {
					break;
				}
				leg.length += top->local.translate.Length();
				top = top->parent;
				leg.chain.insert(leg.chain.begin(), top);
			}
			return leg;
		}

		bool Twins(const Leg& a_one, const Leg& a_other)
		{
			return a_one.chain.front() != a_other.chain.front() && a_one.chain.front()->parent == a_other.chain.front()->parent &&
			       a_one.chain.size() == a_other.chain.size() && std::fabs(a_one.length - a_other.length) <= kTwinTolerance * (std::max)(a_one.length, a_other.length);
		}

		void Place(Leg& a_leg, float a_stance)
		{
			const float stance = a_leg.reach * a_stance, least = a_leg.reach * kNarrowest;
			a_leg.home = Vec3{ a_leg.socket.x, a_leg.socket.y, 0.0f } + a_leg.side * std::sqrt((std::max)(stance * stance - a_leg.socket.z * a_leg.socket.z, least * least));
		}

		void Arrange(std::vector<Leg>& a_legs, const RE::NiAVObject& a_root, const Config::Gait& a_gait)
		{
			const auto  toBody = a_root.world.rotate.Transpose();
			const float scale = (std::max)(a_root.world.scale, 0.05f);
			for (auto& leg : a_legs) {
				const Vec3 foot = (toBody * (leg.chain.back()->world.translate - a_root.world.translate)) * (1.0f / scale);
				leg.socket = (toBody * (leg.chain.front()->world.translate - a_root.world.translate)) * (1.0f / scale);
				leg.side = Unit(Vec3{ foot.x - leg.socket.x, foot.y - leg.socket.y, 0.0f }, Vec3{ foot.x < 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f });
				leg.bend = leg.chain.size() / 2;
				const Vec3& joint = leg.chain[leg.bend]->world.translate;
				leg.reach = ((joint - leg.chain.front()->world.translate).Length() + (leg.chain.back()->world.translate - joint).Length()) / scale;
				const Vec3 out = toBody * (leg.chain.back()->world.translate - leg.chain.front()->parent->world.translate);
				leg.right = out.x > 0.0f;
			}
			Vec3 middle{};
			for (const auto& leg : a_legs) {
				middle += Vec3{ leg.socket.x, leg.socket.y, 0.0f } * (1.0f / static_cast<float>(a_legs.size()));
			}
			for (auto& leg : a_legs) {
				const Vec3 spoke = Unit(Vec3{ leg.socket.x - middle.x, leg.socket.y - middle.y, 0.0f }, leg.side);
				leg.side = Unit(leg.side + (spoke - leg.side) * a_gait.fan, leg.side);
				Place(leg, a_gait.stance);
			}
			for (auto& leg : a_legs) {
				Leg*  twin = nullptr;
				float nearest = 0.0f;
				for (auto& other : a_legs) {
					const float apart = (Vec3{ -other.home.x, other.home.y, 0.0f } - leg.home).Length();
					if (!leg.right && other.right && !other.paired && Twins(leg, other) && (!twin || apart < nearest)) {
						twin = &other;
						nearest = apart;
					}
				}
				if (twin) {
					twin->paired = true;
					const Vec3 hip = (leg.socket + Vec3{ -twin->socket.x, twin->socket.y, twin->socket.z }) * 0.5f;
					const Vec3 side = Unit(leg.side + Vec3{ -twin->side.x, twin->side.y, 0.0f }, leg.side);
					leg.reach = twin->reach = (leg.reach + twin->reach) * 0.5f;
					leg.socket = hip;
					leg.side = side;
					twin->socket = { -hip.x, hip.y, hip.z };
					twin->side = { -side.x, side.y, 0.0f };
					Place(leg, a_gait.stance);
					Place(*twin, a_gait.stance);
				}
			}
			std::ranges::sort(a_legs, [](const Leg& a_one, const Leg& a_other) { return a_one.home.y > a_other.home.y; });
			int rows[2]{};
			for (auto& leg : a_legs) {
				leg.group = (rows[leg.right ? 1 : 0]++ + (leg.right ? 1 : 0)) & 1;
			}
		}

		std::vector<Leg> Detect(RE::NiNode* a_skeleton, const RE::NiAVObject& a_root, const Config::Gait& a_gait)
		{
			std::vector<RE::NiNode*> tips;
			Leaves(a_skeleton, tips);

			std::vector<Leg> limbs;
			for (auto* tip : tips) {
				auto       limb = Limb(tip, a_skeleton);
				const auto same = std::ranges::find(limbs, limb.chain.front(), [](const Leg& a_leg) { return a_leg.chain.front(); });
				if (limb.chain.size() < kFewestBones) {
					continue;
				}
				if (same == limbs.end()) {
					limbs.push_back(std::move(limb));
				} else if (limb.length > same->length) {
					*same = std::move(limb);
				}
			}

			std::vector<Leg> legs;
			float            longest = 0.0f;
			for (const auto& limb : limbs) {
				if (std::ranges::any_of(limbs, [&](const Leg& a_other) { return Twins(limb, a_other); })) {
					legs.push_back(limb);
					longest = (std::max)(longest, limb.length);
				}
			}
			std::erase_if(legs, [&](const Leg& a_leg) { return a_leg.length < longest * kShortFraction; });
			Arrange(legs, a_root, a_gait);
			return legs;
		}

		RE::NiPointer<RE::NiNode> Splice(RE::NiNode* a_bone)
		{
			auto*                         above = a_bone->parent;
			RE::NiPointer<RE::NiAVObject> keep(a_bone);
			RE::NiPointer<RE::NiNode>     helper(RE::NiNode::Create(1));
			if (!above || !helper) {
				return nullptr;
			}
			helper->name = kHelperName;
			above->DetachChild(a_bone);
			above->AttachChild(helper.get(), true);
			helper->AttachChild(a_bone, true);
			return helper;
		}

		void Describe(RE::Actor* a_actor, const Rig& a_rig)
		{
			logs::info("LEGS {:08X} '{}' found {} legs", a_actor->GetFormID(), a_actor->GetName(), a_rig.legs.size());
			for (const auto& leg : a_rig.legs) {
				std::string names;
				for (std::size_t i = 0; i < leg.chain.size(); ++i) {
					names += std::format("{}{}'{}'", i ? " > " : "", i == leg.bend ? "(bend) " : "", leg.chain[i]->name.c_str());
				}
				logs::info("LEGS   {} group {} length {:.0f} reach {:.0f} home ({:.0f},{:.0f}): {}", leg.right ? "right" : "left ", leg.group, leg.length, leg.reach, leg.home.x, leg.home.y, names);
			}
		}

		Rig* RigOf(RE::Actor* a_actor, const Config::Gait& a_gait)
		{
			auto* root = a_actor ? a_actor->Get3D(false) : nullptr;
			if (!root) {
				return nullptr;
			}
			auto& rig = g_rigs[a_actor->GetFormID()];
			if (rig.root == root) {
				return &rig;
			}
			rig = {};
			rig.root = root;
			rig.gait = a_gait;
			auto* found = root->GetObjectByName(RE::BSFixedString(kSkeletonRoot));
			auto* skeleton = found ? found->AsNode() : nullptr;
			if (!skeleton || root->GetObjectByName(RE::BSFixedString(kHelperName))) {
				return &rig;
			}
			rig.legs = Detect(skeleton, *root, a_gait);
			for (auto& leg : rig.legs) {
				leg.knee = Splice(leg.chain[leg.bend]);
				leg.hip = Splice(leg.chain.front());
			}
			std::erase_if(rig.legs, [](const Leg& a_leg) { return !a_leg.hip || !a_leg.knee; });
			if (Config::Get().crawl.log) {
				Describe(a_actor, rig);
			}
			return &rig;
		}

		void Identity(RE::NiNode& a_helper)
		{
			a_helper.local.rotate = RE::NiMatrix3{};
			a_helper.local.translate = {};
			a_helper.local.scale = 1.0f;
		}

		void Pivot(RE::NiNode& a_helper, const RE::NiMatrix3& a_turn, const Vec3& a_joint)
		{
			a_helper.local.rotate = a_turn;
			a_helper.local.translate = a_joint - a_turn * a_joint;
		}

		Pose Below(const Pose& a_parent, const RE::NiNode& a_bone)
		{
			return { a_parent.turn * a_bone.local.rotate, a_parent.at + (a_parent.turn * a_bone.local.translate) * a_parent.scale, a_parent.scale * a_bone.local.scale };
		}

		RE::NiMatrix3 Between(const Vec3& a_from, const Vec3& a_to)
		{
			const Vec3  from = Unit(a_from, Vec3{ 0, 0, 1 }), to = Unit(a_to, from);
			const Vec3  axis = from.Cross(to);
			const float sine = axis.Length();
			return sine < 1e-5f ? RE::NiMatrix3{} : AxisAngle(axis, std::atan2(sine, from.Dot(to)));
		}

		float Reach(Leg& a_leg, const Vec3& a_goal, const Vec3& a_up, float a_blend, float a_arch)
		{
			const auto* above = a_leg.hip->parent;
			const Pose  start{ above->world.rotate, above->world.translate, above->world.scale };
			std::vector<Pose> poses;
			poses.reserve(a_leg.chain.size());
			for (const auto* bone : a_leg.chain) {
				poses.push_back(Below(poses.empty() ? start : poses.back(), *bone));
			}
			const Vec3  hip = poses.front().at, knee = poses[a_leg.bend].at, foot = poses.back().at;
			const Vec3  upper = knee - hip, lower = foot - knee;
			const float a = upper.Length(), b = lower.Length();
			if (a < 1e-3f || b < 1e-3f) {
				return 0.0f;
			}
			const Vec3  target = foot + (a_goal - foot) * a_blend;
			const float now = (foot - hip).Length();
			const float span = (target - hip).Length();
			const float want = std::clamp(span, std::fabs(a - b) + 0.5f, a + b - 0.5f);
			const auto  inner = [&](float a_span) { return std::acos(std::clamp((a * a + b * b - a_span * a_span) / (2.0f * a * b), -1.0f, 1.0f)); };
			const Vec3  hinge = Unit(upper.Cross(lower), Unit(upper.Cross(a_up), Vec3{ 1, 0, 0 }));
			const auto  bendTurn = AxisAngle(hinge, inner(now) - inner(want));
			auto        hipTurn = Between(knee + bendTurn * lower - hip, target - hip);
			if (a_arch > 0.0f) {
				const Vec3 axis = Unit(target - hip, a_up);
				const Vec3 thigh = hipTurn * upper;
				const Vec3 kneeOut = thigh - axis * thigh.Dot(axis), poleOut = a_up - axis * a_up.Dot(axis);
				const Vec3 twist = kneeOut.Cross(poleOut);
				if (kneeOut.Length() > 1e-3f && poleOut.Length() > 1e-3f && twist.Length() > 1e-5f) {
					hipTurn = AxisAngle(twist, std::atan2(twist.Length(), kneeOut.Dot(poleOut)) * a_arch) * hipTurn;
				}
			}

			const auto& aboveTurn = start.turn;
			Pivot(*a_leg.hip, aboveTurn.Transpose() * hipTurn * aboveTurn, a_leg.chain.front()->local.translate);
			const auto& thighTurn = poses[a_leg.bend - 1].turn;
			Pivot(*a_leg.knee, thighTurn.Transpose() * bendTurn * thighTurn, a_leg.chain[a_leg.bend]->local.translate);
			a_leg.span = span;
			a_leg.limit = a + b;
			a_leg.rise = (hip - target).Dot(a_up);
			return std::fabs(span - want);
		}

		Vec3 OnSurface(const Trace::Probe& a_probe, const Frame& a_frame, const Vec3& a_spot)
		{
			const float reach = Config::Get().crawl.footReach * a_frame.scale;
			Trace::Hit  ground;
			if (a_probe.Cast(a_spot + a_frame.normal * reach, a_spot - a_frame.normal * reach, ground) && !ground.tree) {
				return ground.point;
			}
			return a_spot - a_frame.normal * (a_spot - a_frame.point).Dot(a_frame.normal);
		}

		Vec3 Home(const Leg& a_leg, const Frame& a_frame)
		{
			return a_frame.point + (a_frame.right * a_leg.home.x + a_frame.forward * a_leg.home.y) * a_frame.scale;
		}

		Vec3 Step(Rig& a_rig, Leg& a_leg, const Trace::Probe& a_probe, const Frame& a_frame, float a_delta)
		{
			const float share = a_rig.gait.swing;
			const float cycle = a_rig.stride / (1.0f - share);
			const Vec3  home = Home(a_leg, a_frame);
			if (a_rig.fresh) {
				a_leg.planted = OnSurface(a_probe, a_frame, home);
				a_leg.stepping = false;
				a_leg.last = home;
				a_leg.drift = {};
			}
			if (a_delta > 1e-4f) {
				a_leg.drift += ((home - a_leg.last) * (1.0f / a_delta) - a_leg.drift) * (std::min)(1.0f, a_delta * kFollow);
			}
			a_leg.last = home;
			float turn = a_rig.phase - 0.5f * static_cast<float>(a_leg.group);
			turn -= std::floor(turn);
			if (turn >= share) {
				if (a_leg.stepping) {
					a_leg.stepping = false;
					a_leg.planted = a_leg.to;
					++a_rig.steps;
				}
				return a_leg.planted;
			}
			if (!a_leg.stepping) {
				a_leg.stepping = true;
				a_leg.from = a_leg.planted;
			}
			const float swing = turn / share;
			const float ahead = (a_rig.stride * 0.5f + (1.0f - swing) * share * cycle) * (std::min)(1.0f, a_leg.drift.Length() / kSlowest);
			const float height = a_leg.reach * a_frame.scale * a_rig.gait.stepHeight;
			a_leg.to = OnSurface(a_probe, a_frame, home + Unit(a_leg.drift, a_frame.forward) * ahead);
			return a_leg.from + (a_leg.to - a_leg.from) * Ramp(swing, 0.0f, 1.0f) + a_frame.normal * (height * std::sin(kPi * swing));
		}

		void Release(Rig& a_rig)
		{
			for (auto& leg : a_rig.legs) {
				Identity(*leg.hip);
				Identity(*leg.knee);
			}
			a_rig.blend = 0.0f;
			a_rig.fresh = true;
		}

		Rig* Known(RE::Actor* a_actor)
		{
			const auto it = a_actor ? g_rigs.find(a_actor->GetFormID()) : g_rigs.end();
			return it != g_rigs.end() && it->second.root == a_actor->Get3D(false) ? &it->second : nullptr;
		}
	}

	bool Prepare(RE::Actor* a_actor, const Config::Gait& a_gait)
	{
		const auto* rig = RigOf(a_actor, a_gait);
		return rig && !rig->legs.empty();
	}

	void Walk(RE::Actor* a_actor, const Trace::Probe& a_probe, const RE::NiPoint3& a_point, const RE::NiPoint3& a_normal, const RE::NiPoint3& a_forward, float a_delta)
	{
		auto* rig = Known(a_actor);
		if (!rig || rig->legs.empty()) {
			return;
		}
		const Frame frame{ a_point, a_normal, a_forward, Unit(a_forward.Cross(a_normal), Vec3{ 1, 0, 0 }), (std::max)(rig->root->world.scale, 0.05f) };
		rig->touched = true;
		rig->up = a_normal;
		rig->blend = (std::min)(1.0f, rig->blend + a_delta / kBlendSeconds);
		float reach = 0.0f, pace = 0.0f;
		for (const auto& leg : rig->legs) {
			reach += leg.reach;
			pace += leg.drift.Length();
		}
		const float count = static_cast<float>(rig->legs.size());
		if (rig->fresh) {
			pace = 0.0f;
			rig->phase = 0.0f;
		}
		rig->pace = pace / count;
		const float planted = 1.0f - rig->gait.swing;
		rig->stride = (std::max)({ reach / count * frame.scale * rig->gait.stepLength, rig->pace * planted / kFastest, 1.0f });
		const bool  swinging = std::ranges::any_of(rig->legs, &Leg::stepping);
		const float rate = rig->pace < kStill ? 0.0f : rig->pace * planted / rig->stride;
		rig->phase += a_delta * (std::max)(rate, swinging ? kSettle : 0.0f);
		rig->phase -= std::floor(rig->phase);
		for (auto& leg : rig->legs) {
			if (!leg.hip->parent) {
				continue;
			}
			leg.goal = Step(*rig, leg, a_probe, frame, a_delta);
			const float miss = Reach(leg, leg.goal, a_normal, rig->blend, rig->gait.arch);
			leg.reach += (leg.limit / frame.scale - leg.reach) * (std::min)(1.0f, a_delta * kSteady);
			Place(leg, rig->gait.stance);
			if (miss > rig->miss) {
				rig->miss = miss;
				rig->span = leg.span;
				rig->limit = leg.limit;
				rig->rise = leg.rise;
				rig->worst = static_cast<int>(&leg - rig->legs.data());
			}
		}
		rig->fresh = false;
	}

	void Rest(RE::Actor* a_actor, float a_delta)
	{
		auto* rig = Known(a_actor);
		if (!rig || rig->blend <= 0.0f) {
			return;
		}
		rig->touched = true;
		rig->blend -= a_delta / kBlendSeconds;
		if (rig->blend <= 0.0f) {
			Release(*rig);
			return;
		}
		for (auto& leg : rig->legs) {
			if (leg.hip->parent) {
				Reach(leg, leg.goal, rig->up, rig->blend, rig->gait.arch);
			}
		}
	}

	void Relax(RE::Actor* a_actor)
	{
		if (auto* rig = Known(a_actor)) {
			Release(*rig);
		}
	}

	void Sweep()
	{
		for (auto& [id, rig] : g_rigs) {
			if (!rig.touched && rig.blend > 0.0f) {
				Release(rig);
			}
			rig.touched = false;
		}
	}

	Report Summary(RE::Actor* a_actor)
	{
		const auto it = a_actor ? g_rigs.find(a_actor->GetFormID()) : g_rigs.end();
		if (it == g_rigs.end()) {
			return {};
		}
		const Report report{ it->second.steps, it->second.miss, it->second.pace, it->second.worst, it->second.span, it->second.limit, it->second.rise };
		it->second.steps = 0;
		it->second.miss = 0.0f;
		return report;
	}

	void Reset()
	{
		g_rigs.clear();
	}
}
