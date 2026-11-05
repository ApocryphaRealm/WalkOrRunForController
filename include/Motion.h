#pragma once

#include <cstdint>

// The Default Motion choice: run or walk, on a controller too. See the head of source/Motion.cpp.
namespace motion
{
	void Install();           // at kDataLoaded
	void ApplyOnLoad();       // at every load and new game
	void SetWalk(bool a_walk);  // any thread: applies at once, saves the INI

	struct Stats
	{
		bool installed = false;
		std::uint64_t calls = 0;     // the game's asks since it started
		bool lastAnswer = false;     // what the latest ask got: true is run
		bool gamepadEnabled = false;
		bool runFlag = false;        // PlayerControls's own flag
	};
	Stats GetStats();
}
