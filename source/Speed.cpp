// Walk or Run for Controller - own code, GPL-3.0-or-later.
//
// HOW THE SPEED IS APPLIED. The game's movement code reads the player's SpeedMult through the ActorValueOwner vtable: in the 1.5.97
// dump the movement readers (the functions at 0x5D67F0 [id 36355] and 0x60EEF0 [id 37013], and the one holding 0x5ED81A) all do
// `lea rcx,[actor+0xB0]`, `mov edx,0x1E`, `call [rax+8]`, then |value| * 0.01. This replaces slot 1 of the PlayerCharacter's own
// vtable, so the answer changes for the player only, and nothing is stored: no ModAV, no SetBaseAV, no co-save. The value lives in
// this mod's INI and in one float here, so a save made at any setting holds what it would hold without the mod, and the hook being in
// place from the moment the game starts is what "reapplied at every load and new game" comes to. At 100% the game's value passes
// through untouched. SpeedMult covers every gait: 0x60EEF0 multiplies the walk and run columns of every direction by it.
//
// WALK AND RUN HAVE THEIR OWN VALUE (the owner, 2026-11-05). The slider shows and edits the selected Default Motion's; choosing the other
// loads its value at once. Sprint has no value of its own: the same read scales it, and it uses Run's, also while Sprint is held on Walk.
//
// The game holds on to a computed speed until something makes it recompute. Changing CarryWeight makes it do so (Walk Speed Tuner and
// Dynamic Speed Controller use this). Refresh raises the temporary CarryWeight modifier by a little and takes it back in the same
// call, so the player can never briefly read as over-encumbered, and the modifier ends where it began.
#include "PCH.h"

#include "Speed.h"

#include "Motion.h"
#include "Settings.h"
#include "utils/Logger.h"

#include <algorithm>
#include <atomic>

namespace speed
{
	namespace
	{
		std::atomic<float> g_scale{ 1.0f };
		std::atomic<bool> g_installed{ false };
		std::atomic<std::uint64_t> g_hookCount{ 0 };
		std::atomic<float> g_lastBase{ 0.0f };
		std::atomic<float> g_lastApplied{ 0.0f };
		std::atomic<float> g_movementSpeed{ 0.0f };
		std::atomic<int> g_appliedPercent{ 100 };

		int CurrentPercent()
		{
			return settings::walkByDefault.load() && !motion::SprintHeld() ? settings::walkPercent.load() : settings::runPercent.load();
		}

		using GetActorValue_t = float (*)(RE::ActorValueOwner*, RE::ActorValue);
		GetActorValue_t g_original = nullptr;

		// Runs very often, for every actor value of the player, so nothing but the one value does any work, and nothing here logs.
		float GetActorValueHook(RE::ActorValueOwner* a_this, RE::ActorValue a_value)
		{
			const float base = g_original(a_this, a_value);
			if (a_value != RE::ActorValue::kSpeedMult) { return base; }
			const float scale = g_scale.load(std::memory_order_relaxed);
			const float applied = scale == 1.0f ? base : base * scale;
			g_hookCount.fetch_add(1, std::memory_order_relaxed);
			g_lastBase.store(base, std::memory_order_relaxed);
			g_lastApplied.store(applied, std::memory_order_relaxed);
			return applied;
		}
	}

	void Install()
	{
		g_appliedPercent = CurrentPercent();
		g_scale = static_cast<float>(g_appliedPercent.load()) / 100.0f;
		if (g_installed) { return; }
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* owner = player ? player->AsActorValueOwner() : nullptr;
		if (!owner)
		{
			logger::error("speed: the player object is not there at kDataLoaded; the slider cannot change the speed this session");
			return;
		}
		// The instance's own vtable, the one the movement code calls through, so no address has to be looked up on any game version.
		auto* vtable = *reinterpret_cast<std::uintptr_t**>(owner);
		if (!vtable)
		{
			logger::error("speed: the player's ActorValueOwner has no vtable; the slider cannot change the speed this session");
			return;
		}
		g_original = reinterpret_cast<GetActorValue_t>(vtable[1]);
		REL::safe_write(reinterpret_cast<std::uintptr_t>(&vtable[1]), reinterpret_cast<std::uintptr_t>(&GetActorValueHook));
		g_installed = true;
		logger::info("speed: hook on the player's GetActorValue: vtable {:X}, slot 1, original {:X}, owner at player+{:X}; {}%",
			reinterpret_cast<std::uintptr_t>(vtable), reinterpret_cast<std::uintptr_t>(g_original),
			reinterpret_cast<std::uintptr_t>(owner) - reinterpret_cast<std::uintptr_t>(player), g_appliedPercent.load());
	}

	void Refresh()
	{
		auto* tasks = SKSE::GetTaskInterface();
		if (!tasks)
		{
			logger::warn("speed: no SKSE task interface; the new speed takes hold the next time the game recomputes it");
			return;
		}
		tasks->AddTask([] {
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* owner = player ? player->AsActorValueOwner() : nullptr;
			if (!owner) { return; }
			constexpr float kNudge = 0.1f;
#if RUNTIME_LINE == 17
			// slot 6 of the same vtable; CommonLibSSE-NG 7.2 renamed it
			owner->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kCarryWeight, kNudge);
			owner->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kCarryWeight, -kNudge);
#else
			owner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kCarryWeight, kNudge);
			owner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kCarryWeight, -kNudge);
#endif
			logger::debug("speed: refresh done; {}%", g_appliedPercent.load());
		});
	}

	void SetPercent(int a_percent)
	{
		const int percent = std::clamp(a_percent, settings::kPercentMin, settings::kPercentMax);
		const bool walk = settings::walkByDefault.load();
		const int before = (walk ? settings::walkPercent : settings::runPercent).exchange(percent);
		if (before == percent) { return; }
		logger::info("speed: {} {}% -> {}%", walk ? "Walk" : "Run", before, percent);
		settings::Save();
		ApplyCurrent();
	}

	void ApplyCurrent()
	{
		const int percent = CurrentPercent();
		if (g_appliedPercent.exchange(percent) == percent) { return; }
		g_scale = static_cast<float>(percent) / 100.0f;
		logger::debug("speed: {}% in force", percent);
		Refresh();
	}

	Stats GetStats()
	{
		Stats out;
		out.installed = g_installed.load();
		out.hookCount = g_hookCount.load();
		out.lastBase = g_lastBase.load();
		out.lastApplied = g_lastApplied.load();
		out.movementSpeed = g_movementSpeed.load();
		out.appliedPercent = g_appliedPercent.load();
		return out;
	}

	void SampleMovementSpeed()
	{
		auto* tasks = SKSE::GetTaskInterface();
		if (!tasks) { return; }
		tasks->AddTask([] {
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* state = player ? player->AsActorState() : nullptr;
			if (state) { g_movementSpeed = state->DoGetMovementSpeed(); }
		});
	}
}
