// Walk or Run for Controller - own code, GPL-3.0-or-later.
//
// DEFAULT MOTION: RUN OR WALK. The game chooses its walk or run speed in one small function, PlayerControls "is running" (1.5.97 id 41264
// at 0x7051F0; AE and 1.7 id 42343), called from the two places that pick the speed (0x6FBCF2 and 0x6FF83E in 1.5.97):
//
//     return BSInputDeviceManager::IsGamepadEnabled() || data.running;
//
// That first test is why a controller cannot walk: while the gamepad is enabled the answer is always yes, and data.running is not read.
// The Run and Toggle Always Run handlers only flip data.running (0x709370, 0x709690), and the gamepad column of the game's control
// map binds neither of them and does not allow them to be bound. So the pad has no run key at all, and only the Sprint button.
//
// This replaces that function:
// - Run (the default): the same answer as the game's. Nothing changes.
// - Walk with the gamepad enabled: no. The game then takes its own walk speed and animation, the path a keyboard uses with run off.
//   Keyboard keys are not read in this state, as they are not in the game's.
// - Walk without a gamepad: data.running, as in the game, set to false by the setting; Shift and Caps Lock flip it as usual.
// - Walk with the Sprint button held: yes, on either device, as below.
//
// SPRINT ON WALK (the owner, 2026-11-05: holding Sprint did nothing on Walk; read from the 1.5.97 code). The Sprint handler (0x709580) only
// flips PlayerCharacter's isSprinting flag on a press. The player's update (0x6B8F20) starts a sprint while that flag is on and the actor
// is moving forward with stamina, and it also needs the actor already sprinting or its speed (0x5D1190) at or above a point between its
// walk and run speeds. Otherwise it clears the flag. Walking never reaches that speed, so the flag was cleared the frame after the press.
// So on Walk, in the Sprint handler, each frame the button is down: the flag is held on and the is-running answer is yes, so the speed
// climbs and the game's own test starts the sprint; on release the flag goes off and the game's own test ends it. The handler is reached
// through the control map, so a button UVC moved Sprint to works with no key read here, and no press of another button can count.
// On Run nothing here does anything, and the game's press-to-flip stays.
//
// RUN AFTER SPRINT (a Nexus request, 2026-11-06: "when you stop Sprinting, you Run until you stop, then you default to Walk again").
// Default Motion's third choice. On Walk, letting go of Sprint leaves the is-running answer at yes while the move stick or keys still
// push; the first ask that finds no move input ends it, and the walk answer comes back. The run speed's value stays in force meanwhile.
//
// WALK SHEATHED, RUN DRAWN (a Nexus request, 2026-11-07: "walk when sheathed and run when unsheathed/drawn weapon"). Default Motion's
// fourth choice. Each ask reads the player's weapon state (ActorState weaponState at or past Drawn: a weapon, a spell or fists): drawn
// answers run, sheathed is Walk exactly. The Sprint handler works as on Walk in both states. The speed in force follows: Walk's value
// sheathed, Run's drawn, switched the first ask after the state changes. A mod that draws the weapon for you (Auto Draw) needs nothing
// more: drawn is drawn, however it came about.
//
// Nothing is stored in the save. The setting is in the mod's INI; data.running is set again at every load and new game.
#include "PCH.h"

#include "Motion.h"

#include "Settings.h"
#include "Speed.h"
#include "utils/Logger.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>

namespace motion
{
	namespace
	{
		std::atomic<bool> g_installed{ false };
		std::atomic<bool> g_lastAnswer{ false };
		std::atomic<std::uint64_t> g_calls{ 0 };
		std::atomic<bool> g_sprintHeld{ false };
		std::atomic<std::int64_t> g_sprintSeen{ 0 };
		std::atomic<std::uint64_t> g_sprintEvents{ 0 };
		std::atomic<bool> g_carryRun{ false };  // on Walk with run after sprint: a sprint ended and the player has not stopped yet
		constexpr std::int64_t kSprintForgetMs = 300;  // a held button repeats every frame; one that stopped (a menu took the release) is let go

		std::int64_t NowMs()
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		}

		void SetSprintFlag(bool a_on)
		{
			if (auto* player = RE::PlayerCharacter::GetSingleton()) { player->GetPlayerRuntimeData().playerFlags.isSprinting = a_on; }
		}

		void SetSprintHeld(bool a_held)
		{
			if (g_sprintHeld.exchange(a_held) == a_held) { return; }
			if (!a_held) { SetSprintFlag(false); }
			const bool carry = !a_held && settings::RunAfterSprint();
			if (g_carryRun.exchange(carry) != carry && carry) { logger::info("motion: Sprint let go; running until the player stops"); }
			logger::debug("motion: Sprint {} on Walk", a_held ? "held" : "let go");
			speed::ApplyCurrent();
		}

		struct SprintHandlerHook
		{
			static void ProcessButton(RE::SprintHandler* a_this, RE::ButtonEvent* a_event, RE::PlayerControlsData* a_data)
			{
				original(a_this, a_event, a_data);
				if (!a_event || !settings::WalkFamily()) { return; }
				g_sprintEvents.fetch_add(1, std::memory_order_relaxed);
				const bool down = a_event->Value() != 0.0f;
				if (down)
				{
					g_sprintSeen.store(NowMs(), std::memory_order_relaxed);
					SetSprintFlag(true);
				}
				SetSprintHeld(down);
			}
			static inline REL::Relocation<decltype(ProcessButton)> original;
		};

		// The button is down, by the handler's last word and not longer ago than it repeats.
		bool SprintHeldFresh()
		{
			if (!g_sprintHeld.load(std::memory_order_relaxed)) { return false; }
			if (NowMs() - g_sprintSeen.load(std::memory_order_relaxed) <= kSprintForgetMs) { return true; }
			SetSprintHeld(false);
			return false;
		}

		bool GamepadEnabled()
		{
			auto* devices = RE::BSInputDeviceManager::GetSingleton();
			return devices && devices->IsGamepadEnabled();
		}

		// The player is actually moving: the game's own per-frame direction bits on the player's ActorState (all clear when
		// he stands). A plain field read - no game function is called from inside the game's speed computation.
		bool PlayerMoving()
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* state = player ? player->AsActorState() : nullptr;
			if (!state) { return true; }  // nothing to read: leave the decision to the input
			const auto& s1 = state->actorState1;
			return s1.movingBack || s1.movingForward || s1.movingRight || s1.movingLeft;
		}

		// The run after a sprint lasts while there is move input (the stick or the movement keys) AND the player is moving.
		// moveInputVec alone was not enough on Skyrim 1.7.104 (Nexus 1.2.1 test, 2026-11-08): the player stood still with
		// the carry never ending. Its offset is the same on both (read in the code: data at +0x24, running at +0x49, the
		// Movement handler writing moveInputVec at data+0), so the layout is not the cause. PlayerControls zeroes
		// moveInputVec only at the start of an input-event batch (1.5.97 0x704DEA, 1.7.104 0x7AD92C), so it is the last
		// batch's snapshot, not this frame's; the ActorState bits below are this frame's.
		bool CarryRun(RE::PlayerControls* a_controls)
		{
			if (!g_carryRun.load(std::memory_order_relaxed)) { return false; }
			bool input = false;
			if (a_controls && settings::RunAfterSprint())
			{
				const auto& move = a_controls->data.moveInputVec;
				input = move.x * move.x + move.y * move.y > 0.01f;
			}
			const bool moving = input && PlayerMoving();
			if (moving) { return true; }
			g_carryRun.store(false, std::memory_order_relaxed);
			logger::info("motion: the player stopped ({}); walking again", input ? "move input left over, ActorState not moving" : "no move input");
			speed::ApplyCurrent();
			return false;
		}

		// Choice 3: the player's weapon state, read at each ask; a change switches the speed in force and the slider's name.
		void UpdateDrawn()
		{
			if (settings::MotionValue() != settings::kWalkSheathed) { return; }
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* state = player ? player->AsActorState() : nullptr;
			const bool drawn = state && state->IsWeaponDrawn();
			if (settings::weaponDrawn.exchange(drawn) == drawn) { return; }
			logger::debug("motion: weapon {}; {}", drawn ? "drawn" : "sheathed", drawn ? "running" : "walking");
			speed::ApplyCurrent();
		}

		bool IsRunningHook(RE::PlayerControls* a_controls)
		{
			UpdateDrawn();
			const int choice = settings::MotionValue();
			bool answer = false;
			if (choice != settings::kRun && (SprintHeldFresh() || CarryRun(a_controls))) { answer = true; }
			else if (choice == settings::kRun) { answer = GamepadEnabled() || (a_controls && a_controls->data.running); }
			else if (!settings::WalksNow()) { answer = true; }  // choice 3, drawn
			else if (GamepadEnabled()) { answer = false; }
			else { answer = a_controls && a_controls->data.running; }
			g_calls.fetch_add(1, std::memory_order_relaxed);
			g_lastAnswer.store(answer, std::memory_order_relaxed);
			return answer;
		}

		void SetRunFlag(bool a_running)
		{
			auto* controls = RE::PlayerControls::GetSingleton();
			if (!controls)
			{
				logger::warn("motion: no PlayerControls yet; the run flag is set at the next load");
				return;
			}
			controls->data.running = a_running;
		}
	}

	void Install()
	{
		if (g_installed) { return; }
		REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(41264, 42343) };
		// Read in the 1.5.97 and 1.7.104 code: the function starts the same on both. Anything else is not this function, so it stays alone.
		static constexpr std::array<std::uint8_t, 12> kStart{ 0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xD9, 0x48, 0x8B, 0x0D };
		if (std::memcmp(reinterpret_cast<const void*>(target.address()), kStart.data(), kStart.size()) != 0)
		{
			logger::error("motion: the function at {:X} is not the one expected; the Walk choice does nothing on a controller this session", target.address());
			return;
		}
		SKSE::AllocTrampoline(14);
		SKSE::GetTrampoline().write_branch<5>(target.address(), &IsRunningHook);
		REL::Relocation<std::uintptr_t> sprintVtable{ RE::VTABLE_SprintHandler[0] };
#if RUNTIME_LINE == 17
		// Skyrim 1.7 put two new functions into every player input handler's vtable at slots 4 and 5 (both `ret 0`), so
		// ProcessButton moved to slot 6 (read in the 1.7.104 code: SprintHandler slot 6 is the 1.5.97 handler's body, flag at
		// +0xBED). CommonLibSSE-NG 7.2 still names slot 4; hooking it hooked a function the game never calls, and on 1.7 the
		// Sprint button did nothing from a walk (a Nexus report, 2026-11-08). SE 1.5.97 and AE 1.6.1170 keep slot 4.
		constexpr std::size_t kProcessButtonSlot = 6;
#else
		constexpr std::size_t kProcessButtonSlot = 4;
#endif
		SprintHandlerHook::original = sprintVtable.write_vfunc(kProcessButtonSlot, SprintHandlerHook::ProcessButton);
		g_installed = true;
		logger::info("motion: hook on PlayerControls's is-running test at {:X}; Default Motion {}", target.address(), settings::MotionValue());
	}

	void ApplyOnLoad()
	{
		if (!settings::WalkFamily()) { return; }  // Run: the game's own flag stays as the game set it
		SetRunFlag(false);
		logger::debug("motion: a walking choice: the run flag is off");
	}

	void SetMotion(int a_value)
	{
		const int value = std::clamp(a_value, 0, static_cast<int>(settings::kMotionMax));
		const int before = settings::defaultMotion.exchange(value);
		if (before == value) { return; }
		if (value != settings::kWalkRunAfterSprint) { g_carryRun.store(false); }
		if (value != settings::kWalkSheathed) { settings::weaponDrawn.store(false); }
		else if (auto* tasks = SKSE::GetTaskInterface()) { tasks->AddTask([] { UpdateDrawn(); }); }
		logger::info("motion: Default Motion {} -> {}; the slider now holds the {} value, {}%", before, value, settings::WalksNow() ? "Walk" : "Run", settings::SelectedPercent());
		settings::Save();
		speed::ApplyCurrent();
		const bool walkFamily = value != settings::kRun;
		if (auto* tasks = SKSE::GetTaskInterface())
		{
			tasks->AddTask([walkFamily] { SetRunFlag(!walkFamily); });
		}
	}

	bool SprintHeld() { return g_sprintHeld.load(std::memory_order_relaxed) || g_carryRun.load(std::memory_order_relaxed); }

	Stats GetStats()
	{
		Stats out;
		out.installed = g_installed.load();
		out.calls = g_calls.load();
		out.lastAnswer = g_lastAnswer.load();
		out.gamepadEnabled = GamepadEnabled();
		auto* controls = RE::PlayerControls::GetSingleton();
		out.runFlag = controls && controls->data.running;
		out.sprintHeld = g_sprintHeld.load();
		out.sprintEvents = g_sprintEvents.load();
		return out;
	}
}
