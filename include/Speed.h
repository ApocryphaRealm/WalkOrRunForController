#pragma once

#include <cstdint>

// The player's movement speed: a hook on the player's own read of SpeedMult. See the head of source/Speed.cpp.
namespace speed
{
	void Install();                  // at kDataLoaded, when the player object exists
	void SetPercent(int a_percent);  // any thread: clamps, applies at once, saves the INI. Nothing when the value is unchanged.
	void Refresh();                  // has the game take the speed again, on its own thread

	struct Stats
	{
		bool installed = false;
		std::uint64_t hookCount = 0;  // SpeedMult reads answered since the game started
		float lastBase = 0.0f;        // the game's own value at the latest read
		float lastApplied = 0.0f;     // what the hook answered
		float movementSpeed = 0.0f;   // the player's movement speed, sampled on the game's thread at the previous state call
	};
	Stats GetStats();
	void SampleMovementSpeed();  // queues the sample for the next state call
}
