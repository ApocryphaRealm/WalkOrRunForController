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
// Nothing is stored in the save. The setting is in the mod's INI; data.running is set again at every load and new game.
#include "PCH.h"

#include "Motion.h"

#include "Settings.h"
#include "Speed.h"
#include "utils/Logger.h"

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
			logger::debug("motion: Sprint {} on Walk", a_held ? "held" : "let go");
			speed::ApplyCurrent();
		}

		struct SprintHandlerHook
		{
			static void ProcessButton(RE::SprintHandler* a_this, RE::ButtonEvent* a_event, RE::PlayerControlsData* a_data)
			{
				original(a_this, a_event, a_data);
				if (!a_event || !settings::walkByDefault.load(std::memory_order_relaxed)) { return; }
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

		bool IsRunningHook(RE::PlayerControls* a_controls)
		{
			bool answer = false;
			if (settings::walkByDefault.load(std::memory_order_relaxed) && SprintHeldFresh()) { answer = true; }
			else if (GamepadEnabled()) { answer = !settings::walkByDefault.load(std::memory_order_relaxed); }
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
		SprintHandlerHook::original = sprintVtable.write_vfunc(4, SprintHandlerHook::ProcessButton);
		g_installed = true;
		logger::info("motion: hook on PlayerControls's is-running test at {:X}; {}", target.address(), settings::walkByDefault.load() ? "Walk" : "Run");
	}

	void ApplyOnLoad()
	{
		if (!settings::walkByDefault.load()) { return; }  // Run: the game's own flag stays as the game set it
		SetRunFlag(false);
		logger::debug("motion: Walk: the run flag is off");
	}

	void SetWalk(bool a_walk)
	{
		const bool before = settings::walkByDefault.exchange(a_walk);
		if (before == a_walk) { return; }
		logger::info("motion: {} -> {}; the slider now holds the {} value, {}%", before ? "Walk" : "Run", a_walk ? "Walk" : "Run", a_walk ? "Walk" : "Run", settings::SelectedPercent());
		settings::Save();
		speed::ApplyCurrent();
		if (auto* tasks = SKSE::GetTaskInterface())
		{
			tasks->AddTask([a_walk] { SetRunFlag(!a_walk); });
		}
	}

	bool SprintHeld() { return g_sprintHeld.load(std::memory_order_relaxed); }

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
