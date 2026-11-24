#pragma once

namespace Creature
{
	inline bool Usable(RE::Actor* a_actor)
	{
		return a_actor && a_actor->Is3DLoaded() && !a_actor->IsDead() && !a_actor->IsInRagdollState() && !a_actor->IsInKillMove() && a_actor->GetCharController();
	}

	inline float Scale(RE::Actor* a_actor)
	{
		const auto* root = a_actor->Get3D(false);
		return root && root->world.scale > 0.05f ? root->world.scale : 1.0f;
	}

	inline void Hold(RE::Actor* a_actor, RE::hkpCharacterStateType a_state)
	{
		if (auto* controller = a_actor->GetCharController()) {
			controller->context.currentState = a_state;
			controller->SetLinearVelocityImpl(RE::hkVector4(0, 0, 0, 0));
			controller->fallStartHeight = a_actor->GetPosition().z;
			controller->fallTime = 0.0f;
		}
	}

	inline std::string Lower(std::string a_text)
	{
		for (auto& ch : a_text) {
			ch = ch == '/' ? '\\' : static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
		}
		return a_text;
	}

	inline std::string SkeletonPath(RE::Actor* a_actor)
	{
		const auto* race = a_actor->GetRace();
		if (!race) {
			return {};
		}
		const auto* base = a_actor->GetActorBase();
		const auto  sex = base && base->GetSex() == RE::SEX::kFemale ? RE::SEX::kFemale : RE::SEX::kMale;
		const char* path = race->skeletonModels[std::to_underlying(sex)].GetModel();
		return Lower(path ? path : "");
	}

	inline std::string RaceID(RE::Actor* a_actor)
	{
		const auto* race = a_actor->GetRace();
		const char* id = race ? race->GetFormEditorID() : nullptr;
		return Lower(id ? id : "");
	}

	class Kinds
	{
	public:
		template <class Classify>
		int Of(RE::Actor* a_actor, Classify&& a_classify)
		{
			auto* root = a_actor->Get3D(false);
			if (!root) {
				return -1;
			}
			auto& seen = m_seen[a_actor->GetFormID()];
			if (seen.root != root) {
				seen = { root, a_classify(a_actor) };
			}
			return seen.kind;
		}

		void Clear() { m_seen.clear(); }

	private:
		struct Seen
		{
			RE::NiAVObject* root = nullptr;
			int             kind = -1;
		};

		std::unordered_map<RE::FormID, Seen> m_seen;
	};

	struct Chase
	{
		RE::FormID target = 0;
		float      closest = 0.0f;
		float      closestAt = 0.0f;

		void Note(RE::FormID a_target, float a_distance, float a_now)
		{
			if (a_target != target || a_distance < closest - kGain) {
				target = a_target;
				closest = a_distance;
				closestAt = a_now;
			}
		}

		[[nodiscard]] bool Stalled(float a_now, float a_window) const { return a_now - closestAt > a_window; }

	private:
		static constexpr float kGain = 40.0f;
	};

	template <class Visit>
	void EachFighter(float a_reach, Visit&& a_visit)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* lists = RE::ProcessLists::GetSingleton();
		if (!player || !lists) {
			return;
		}
		const RE::NiPoint3 centre = player->GetPosition();
		for (auto& handle : lists->highActorHandles) {
			const auto held = handle.get();
			auto*      actor = held.get();
			if (!actor || actor == player || !actor->IsInCombat() || !Usable(actor) || (actor->GetPosition() - centre).Length() > a_reach) {
				continue;
			}
			const auto targetHeld = actor->GetActorRuntimeData().currentCombatTarget.get();
			auto*      target = targetHeld.get();
			if (!target || target->IsDead() || !target->Is3DLoaded()) {
				continue;
			}
			if (!a_visit(actor, target)) {
				return;
			}
		}
	}
}
