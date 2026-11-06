#pragma once

#include <cstdint>

// The Default Motion choice: run or walk, on a controller too. See the head of source/Motion.cpp.
namespace motion
{
	void Install();           // at kDataLoaded
	void ApplyOnLoad();       // at every load and new game
	void SetWalk(bool a_walk);  // any thread: applies at once, saves the INI
	void SetMotion(int a_value);  // the Default Motion row: 0 Run, 1 Walk, 2 Walk with a run after each sprint
	bool SprintHeld();          // on Walk: the player's Sprint button is down now, or a run after a sprint is going on

	struct Stats
	{
		bool installed = false;
		std::uint64_t calls = 0;     // the game's asks since it started
		bool lastAnswer = false;     // what the latest ask got: true is run
		bool gamepadEnabled = false;
		bool runFlag = false;        // PlayerControls's own flag
		bool sprintHeld = false;     // on Walk: the Sprint button is down
		std::uint64_t sprintEvents = 0;  // button events the game gave its Sprint handler on Walk
	};
	Stats GetStats();
}
