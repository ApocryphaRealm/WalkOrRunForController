#pragma once

#include <cstdint>

// The player's movement speed: a hook on the player's own read of SpeedMult. See the head of source/Speed.cpp.
namespace speed
{
	void Install();                  // at kDataLoaded, when the player object exists
	void SetPercent(int a_percent);  // any thread: the selected Default Motion's value. Clamps, applies at once, saves the INI. Nothing when unchanged.
	void ApplyCurrent();             // takes the value for what the player is doing now; has the game take it only when it changed
	void Refresh();                  // has the game take the speed again, on its own thread

	struct Stats
	{
		bool installed = false;
		std::uint64_t hookCount = 0;  // SpeedMult reads answered since the game started
		float lastBase = 0.0f;        // the game's own value at the latest read
		float lastApplied = 0.0f;     // what the hook answered
		float movementSpeed = 0.0f;   // the player's movement speed, sampled on the game's thread at the previous state call
		int appliedPercent = 100;     // the value in force: Run's while Sprint is held on Walk
	};
	Stats GetStats();
	void SampleMovementSpeed();  // queues the sample for the next state call
}
