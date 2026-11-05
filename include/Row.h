#pragma once

#include <string>

// The slider's row in the journal's own Gameplay settings list, added at run time. See the head of source/Row.cpp.
namespace row
{
	void Install();  // hooks JournalMenu::AdvanceMovie

	struct State
	{
		bool present = false;    // the row is in the list this journal is showing
		int index = -1;          // its place in the list
		int clip = -1;           // the on-screen clip drawing it, -1 when scrolled away
		int entries = 0;         // rows in the list
		double entryValue = -1;  // the value the list holds for it
		std::string listPath;    // the movie path of the list, empty until found
	};
	State GetState();
}
