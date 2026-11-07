#pragma once

#include <atomic>
#include <cstdint>
#include <string>

// The mod's settings: read from SKSE\Plugins\MovementSpeedSlider.ini, written back line by line (never with
// WritePrivateProfileString, which reports success and writes nothing under MO2's virtual folder). The journal and DevBench run on
// different threads, so every value is atomic. Compiled defaults match the shipped INI (rule 16).
namespace settings
{
	struct Defaults
	{
		static constexpr std::uint32_t logLevel = 2;
		static constexpr int runPercent = 100;
		static constexpr int walkPercent = 100;
		static constexpr int defaultMotion = 0;  // Run
	};

	inline constexpr int kPercentMin = 25;
	inline constexpr int kPercentMax = 150;

	inline std::atomic<std::uint32_t> logLevel{ Defaults::logLevel };
	inline std::atomic<int> runPercent{ Defaults::runPercent };    // iRunSpeedPercent:General - the slider while Default Motion is Run
	inline std::atomic<int> walkPercent{ Defaults::walkPercent };  // iWalkSpeedPercent:General - the slider while Default Motion is Walk
	// The Default Motion row's value, iDefaultMotion:General (1.2.0; it replaced bWalkByDefault and bRunAfterSprint, which an old INI
	// still starts it from): 0 Run, 1 Walk, 2 Walk with a run after each sprint, 3 Walk while sheathed and run while drawn.
	enum Motion : int { kRun = 0, kWalk = 1, kWalkRunAfterSprint = 2, kWalkSheathed = 3 };
	inline constexpr int kMotionMax = kWalkSheathed;
	inline std::atomic<int> defaultMotion{ Defaults::defaultMotion };
	// The player's weapon or magic is out (fists too), as the motion hook last read it. Only choice 3 looks at it.
	inline std::atomic<bool> weaponDrawn{ false };

	inline int MotionValue() { return defaultMotion.load(); }
	// Any of the walking choices: the Sprint handler hook works and the game's run flag is held off.
	inline bool WalkFamily() { return defaultMotion.load() != kRun; }
	inline bool RunAfterSprint() { return defaultMotion.load() == kWalkRunAfterSprint; }
	// The player walks now when nothing is held: Walk, Walk-run-after-sprint, or the sheathed half of choice 3.
	inline bool WalksNow()
	{
		const int m = defaultMotion.load();
		return m == kWalk || m == kWalkRunAfterSprint || (m == kWalkSheathed && !weaponDrawn.load());
	}

	// The value the slider shows and edits: the one in force now, Walk's or Run's.
	inline int SelectedPercent() { return WalksNow() ? walkPercent.load() : runPercent.load(); }

	void Load();  // at plugin load; a missing INI or line keeps the default
	bool Save();  // every value, keeping the file's comments; false when the file cannot be written. Any thread.
	const std::string& IniPath();
	void ApplyLogLevel();
}
