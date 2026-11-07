// Walk or Run for Controller - own code, GPL-3.0-or-later.
//
// THE ROW. The journal's System page builds its Gameplay list in ActionScript: onSettingsCategoryPress gives the list an array of
// {text, movieType} objects, the game's RequestGameplayOptions fills in ID and value, rows left without an ID are removed, and the
// list draws what remains (SystemPage.as, SettingsOptionItem.as, OptionsList.as, BSScrollingList.as). Vanilla's, SkyUI 5.2's, Quest
// Journal Overhaul's and Norden UI Black's journals all do this with the same classes, so no movie has to be replaced:
//
// - After each journal frame this looks at that list. If it holds "$Difficulty" and not our row, our row goes in beneath Difficulty,
//   as one more plain object with movieType 0 (the game's own slider, the one Look Sensitivity uses). Default Motion goes beneath it,
//   with movieType 1 (the game's own option stepper, the one Difficulty uses).
// - A press on the row is handled entirely by the movie's own code: SettingsOptionItem moves its scroll bar one position, and
//   OptionsList.onValueChange writes the new value into our object. This only reads the object back, once a frame.
// - That same code also calls the game's OptionChange with the row's ID. The handler (1.5.97 0x8F2EB0, 1.7.104 0x9AB260) switches on
//   IDs up to 0x1A / 0x1B; any higher ID is looked up as a form, and acted on only when it is a sound category (type 0x82). kRowId is
//   above the switch and in load order 0x7F, so the call does nothing on either version.
// - Y (Defaults) works too: ResetSettingsToDefaults takes `defaultVal`, which our row carries.
//
// THE STEPS. The row's scroll bar is built with 0..20 positions and value = position / 20, for every row. One percent per press takes
// 126 positions, so while a clip draws our row its bar gets 0..125, and the value in our object stays in the movie's own unit
// (position / 20). When the list scrolls and the clip draws another row, its bar goes back to 0..20.
//
// Every member here that is a getter or setter in ActionScript goes through movie->SetVariable, which is known to run the setter
// (Unbind Vanilla Controls sets SystemPage.currentState that way). The list's array is read from its plain field EntriesA.
#include "PCH.h"

#include "Row.h"

#include "Motion.h"
#include "Settings.h"
#include "Speed.h"
#include "Strings.h"
#include "utils/Logger.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <mutex>
#include <string>

namespace row
{
	namespace
	{
		// vanilla's instance names first, then the shapes a replacer that wraps the hierarchy is known to use
		constexpr std::array kPages = {
			"_root.QuestJournalFader.Menu_mc.SystemFader.Page_mc",
			"_root.Menu_mc.SystemFader.Page_mc",
			"_level0.QuestJournalFader.Menu_mc.SystemFader.Page_mc",
		};
		constexpr const char* kListMember = ".OptionsListsPanel.OptionsLists.List_mc";
		constexpr std::string_view kDifficulty = "$Difficulty";
		constexpr const char* kOwnClipMark = "_mspRow";
		constexpr double kRowId = 0x7FFFFFF0;
		constexpr double kMotionId = 0x7FFFFFF1;  // the same reasoning as kRowId
		constexpr int kMaxClips = 64;
		constexpr double kRowPageSize = 0.7 * (settings::kPercentMax - settings::kPercentMin) / 20.0;  // the same thumb as the 0..20 rows

		std::mutex g_lock;
		State g_state;  // g_lock

		int g_entryPercent = -1;  // main thread: the percent last seen in our object; another value there means the player moved it
		int g_entryWalk = -1;     // main thread: the same for Default Motion: 0 to settings::kMotionMax
		int g_rowTextWalk = -1;   // main thread: which of the two the row's name last said

		double ValueOf(int a_percent) { return static_cast<double>(a_percent - settings::kPercentMin) / 20.0; }

		int PercentOf(double a_value)
		{
			return std::clamp(static_cast<int>(std::lround(a_value * 20.0)) + settings::kPercentMin, settings::kPercentMin, settings::kPercentMax);
		}

		// Names which value it is editing: the selected Default Motion's.
		std::string RowText(int a_percent)
		{
			return std::format("{} {}%", settings::WalksNow() ? TR("MSP_RowWalk", "Walk Speed") : TR("MSP_RowRun", "Run Speed"), a_percent);
		}

		void Publish(State a_state)
		{
			std::scoped_lock l(g_lock);
			g_state = std::move(a_state);
		}

		// The list on this journal's System page, its path written to a_path. Not there is normal: the page is still loading (rule 17).
		bool FindList(RE::GFxMovieView* a_movie, RE::GFxValue& a_list, std::string& a_path)
		{
			for (const char* page : kPages)
			{
				std::string path = std::string(page) + kListMember;
				if (a_movie->GetVariable(&a_list, path.c_str()) && a_list.IsObject())
				{
					a_path = std::move(path);
					return true;
				}
			}
			return false;
		}

		bool EntryMember(const RE::GFxValue& a_entry, const char* a_name, RE::GFxValue& a_out)
		{
			return a_entry.IsObject() && a_entry.GetMember(a_name, &a_out);
		}

		void InsertRow(RE::GFxMovieView* a_movie, RE::GFxValue& a_entries, const std::string& a_listPath, std::uint32_t a_afterIndex)
		{
			const int percent = settings::SelectedPercent();
			const std::string text = RowText(percent);
			g_rowTextWalk = settings::WalksNow() ? 1 : 0;
			RE::GFxValue entry;
			a_movie->CreateObject(&entry);
			if (!entry.IsObject())
			{
				logger::warn("row: the movie could not make the row's object");
				return;
			}
			entry.SetMember("text", RE::GFxValue(text.c_str()));
			entry.SetMember("movieType", RE::GFxValue(0.0));
			entry.SetMember("ID", RE::GFxValue(kRowId));
			entry.SetMember("value", RE::GFxValue(ValueOf(percent)));
			entry.SetMember("defaultVal", RE::GFxValue(ValueOf(settings::Defaults::runPercent)));

			std::array<RE::GFxValue, 3> args{ RE::GFxValue(static_cast<double>(a_afterIndex + 1)), RE::GFxValue(0.0), entry };
			if (!a_entries.Invoke("splice", nullptr, args.data(), args.size()))
			{
				logger::debug("row: splice failed; the row goes at the end of the list");
				a_entries.PushBack(entry);
			}
			g_entryPercent = percent;
			a_movie->Invoke((a_listPath + ".InvalidateData").c_str(), nullptr, nullptr, 0);
			logger::info("row: added to the Gameplay list beneath Difficulty (row {}), {}%", a_afterIndex + 1, percent);
		}

		// Default Motion: the game's own option stepper, the kind Difficulty uses. Value 0 is Run, 1 is Walk.
		void InsertMotionRow(RE::GFxMovieView* a_movie, RE::GFxValue& a_entries, const std::string& a_listPath, std::uint32_t a_afterIndex)
		{
			const int motionValue = settings::MotionValue();
			const std::string text = TR("MSP_Motion", "Default Motion");
			const std::string run = TR("MSP_Run", "Run");
			const std::string walking = TR("MSP_Walk", "Walk");
			const std::string walkRun = TR("MSP_WalkRunAfterSprint", "Walk, run after sprint");
			const std::string walkSheathed = TR("MSP_WalkSheathed", "Walk sheathed, run drawn");
			RE::GFxValue entry;
			RE::GFxValue options;
			a_movie->CreateObject(&entry);
			a_movie->CreateArray(&options);
			if (!entry.IsObject() || !options.IsArray())
			{
				logger::warn("row: the movie could not make the Default Motion row's objects");
				return;
			}
			options.PushBack(RE::GFxValue(run.c_str()));
			options.PushBack(RE::GFxValue(walking.c_str()));
			options.PushBack(RE::GFxValue(walkRun.c_str()));
			options.PushBack(RE::GFxValue(walkSheathed.c_str()));
			entry.SetMember("text", RE::GFxValue(text.c_str()));
			entry.SetMember("movieType", RE::GFxValue(1.0));
			entry.SetMember("options", options);
			entry.SetMember("ID", RE::GFxValue(kMotionId));
			entry.SetMember("value", RE::GFxValue(static_cast<double>(settings::MotionValue())));
			entry.SetMember("defaultVal", RE::GFxValue(static_cast<double>(settings::Defaults::defaultMotion)));

			std::array<RE::GFxValue, 3> args{ RE::GFxValue(static_cast<double>(a_afterIndex + 1)), RE::GFxValue(0.0), entry };
			if (!a_entries.Invoke("splice", nullptr, args.data(), args.size()))
			{
				logger::debug("row: splice failed; Default Motion goes at the end of the list");
				a_entries.PushBack(entry);
			}
			g_entryWalk = settings::MotionValue();
			a_movie->Invoke((a_listPath + ".InvalidateData").c_str(), nullptr, nullptr, 0);
			logger::info("row: Default Motion added beneath the slider (row {}), choice {}", a_afterIndex + 1, motionValue);
		}

		void SetScrollRange(RE::GFxMovieView* a_movie, const std::string& a_clipPath, double a_pageSize, double a_max)
		{
			std::array<RE::GFxValue, 3> args{ RE::GFxValue(a_pageSize), RE::GFxValue(0.0), RE::GFxValue(a_max) };
			a_movie->Invoke((a_clipPath + ".ScrollBar_mc.setScrollProperties").c_str(), nullptr, args.data(), args.size());
		}

		// The on-screen clip that draws our row, -1 when it is scrolled away. Gives each clip the scroll range of the row it draws now.
		int UpdateClips(RE::GFxMovieView* a_movie, RE::GFxValue& a_list, const std::string& a_listPath, int a_percent, const std::string& a_text, bool a_textChanged, bool a_motionChanged)
		{
			int shown = -1;
			for (int i = 0; i < kMaxClips; ++i)
			{
				const std::string name = std::format("Entry{}", i);
				RE::GFxValue clip;
				if (!a_list.GetMember(name.c_str(), &clip) || !clip.IsObject()) { break; }
				RE::GFxValue id;
				const bool ours = clip.GetMember("iID", &id) && id.IsNumber() && id.GetNumber() == kRowId;
				RE::GFxValue mark;
				const bool marked = clip.GetMember(kOwnClipMark, &mark) && mark.IsBool() && mark.GetBool();
				const std::string clipPath = a_listPath + "." + name;
				if (a_motionChanged && id.IsNumber() && id.GetNumber() == kMotionId)
				{
					a_movie->SetVariable((clipPath + ".value").c_str(), RE::GFxValue(static_cast<double>(settings::MotionValue())));
				}
				if (ours)
				{
					shown = i;
					if (!marked)
					{
						SetScrollRange(a_movie, clipPath, kRowPageSize, settings::kPercentMax - settings::kPercentMin);
						clip.SetMember(kOwnClipMark, RE::GFxValue(true));
						logger::debug("row: clip {} now draws the row", i);
					}
					RE::GFxValue position;
					const double wanted = a_percent - settings::kPercentMin;
					const bool hasPosition = a_movie->GetVariable(&position, (clipPath + ".ScrollBar_mc._position").c_str()) && position.IsNumber();
					// The list wrote the value while this clip's bar still ran 0..20, or the percent came from elsewhere.
					if (!hasPosition || position.GetNumber() != wanted)
					{
						a_movie->SetVariable((clipPath + ".value").c_str(), RE::GFxValue(ValueOf(a_percent)));
					}
					if (!marked || a_textChanged)
					{
						a_movie->SetVariable((clipPath + ".text").c_str(), RE::GFxValue(a_text.c_str()));
					}
				}
				else if (marked)
				{
					SetScrollRange(a_movie, clipPath, 0.7, 20.0);
					clip.SetMember(kOwnClipMark, RE::GFxValue(false));
					logger::debug("row: clip {} went back to the game's 0..20", i);
				}
			}
			return shown;
		}

		void OnFrame(RE::GFxMovieView* a_movie)
		{
			static bool loggedNoList = false;
			static bool loggedList = false;

			State state;
			RE::GFxValue list;
			RE::GFxValue entries;
			std::string listPath;
			if (!FindList(a_movie, list, listPath))
			{
				if (!loggedNoList)
				{
					loggedNoList = true;
					logger::debug("row: no System page list yet");
				}
				Publish({});
				return;
			}
			if (!loggedList)
			{
				loggedList = true;
				logger::info("row: the settings list is at {}", listPath);
			}
			state.listPath = listPath;
			if (!list.GetMember("EntriesA", &entries) || !entries.IsArray())
			{
				Publish(std::move(state));
				return;
			}

			const auto count = entries.GetArraySize();
			std::int64_t rowIndex = -1;
			std::int64_t difficulty = -1;
			std::int64_t motionIndex = -1;
			RE::GFxValue rowEntry;
			RE::GFxValue motionEntry;
			for (std::uint32_t i = 0; i < count; ++i)
			{
				RE::GFxValue entry;
				if (!entries.GetElement(i, &entry)) { continue; }
				RE::GFxValue member;
				if (EntryMember(entry, "ID", member) && member.IsNumber() && member.GetNumber() == kRowId)
				{
					rowIndex = i;
					rowEntry = entry;
					continue;
				}
				if (EntryMember(entry, "ID", member) && member.IsNumber() && member.GetNumber() == kMotionId)
				{
					motionIndex = i;
					motionEntry = entry;
					continue;
				}
				if (EntryMember(entry, "text", member) && member.IsString() && member.GetString() && kDifficulty == member.GetString())
				{
					difficulty = i;
				}
			}

			if (rowIndex < 0 && difficulty >= 0)
			{
				InsertRow(a_movie, entries, listPath, static_cast<std::uint32_t>(difficulty));
				// Found on the next frame, from the array itself, whether the splice or the PushBack put it there.
				state.entries = static_cast<int>(entries.GetArraySize());
				Publish(std::move(state));
				return;
			}

			if (rowIndex >= 0 && motionIndex < 0)
			{
				InsertMotionRow(a_movie, entries, listPath, static_cast<std::uint32_t>(rowIndex));
				state.entries = static_cast<int>(entries.GetArraySize());
				Publish(std::move(state));
				return;
			}

			state.entries = static_cast<int>(count);
			if (rowIndex < 0)
			{
				// Not the Gameplay list (Display, Audio): leave it alone, and hand any clip we marked back.
				UpdateClips(a_movie, list, listPath, settings::SelectedPercent(), {}, false, false);
				Publish(std::move(state));
				return;
			}

			RE::GFxValue value;
			int entryPercent = settings::SelectedPercent();
			if (EntryMember(rowEntry, "value", value) && value.IsNumber()) { entryPercent = PercentOf(value.GetNumber()); }

			bool changed = false;
			if (entryPercent != g_entryPercent)  // the player, through the movie's own code
			{
				g_entryPercent = entryPercent;
				speed::SetPercent(entryPercent);
				changed = true;
			}
			const int percent = settings::SelectedPercent();
			if (percent != entryPercent)  // the other Default Motion's value, or DevBench, from outside the list
			{
				rowEntry.SetMember("value", RE::GFxValue(ValueOf(percent)));
				g_entryPercent = percent;
				changed = true;
			}
			if (const int selected = settings::WalksNow() ? 1 : 0; selected != g_rowTextWalk)  // the name, even where the two values match
			{
				g_rowTextWalk = selected;
				changed = true;
			}
			const std::string text = RowText(percent);
			if (changed) { rowEntry.SetMember("text", RE::GFxValue(text.c_str())); }

			// Default Motion, the same two ways round
			RE::GFxValue motionValue;
			bool motionChanged = false;
			int entryWalk = settings::MotionValue();
			if (EntryMember(motionEntry, "value", motionValue) && motionValue.IsNumber()) { entryWalk = std::clamp(static_cast<int>(std::lround(motionValue.GetNumber())), 0, settings::kMotionMax); }
			if (entryWalk != g_entryWalk)
			{
				g_entryWalk = entryWalk;
				motion::SetMotion(entryWalk);
			}
			const int walk = settings::MotionValue();
			if (walk != entryWalk)
			{
				motionEntry.SetMember("value", RE::GFxValue(static_cast<double>(walk)));
				g_entryWalk = walk;
				motionChanged = true;
			}

			state.motionPresent = true;
			state.motionIndex = static_cast<int>(motionIndex);
			state.present = true;
			state.index = static_cast<int>(rowIndex);
			state.clip = UpdateClips(a_movie, list, listPath, percent, text, changed, motionChanged);
			state.entryValue = ValueOf(percent);
			Publish(std::move(state));
		}

		struct AdvanceMovie
		{
			static void thunk(RE::JournalMenu* a_this, float a_interval, std::uint32_t a_currentTime)
			{
				func(a_this, a_interval, a_currentTime);
				if (!a_this || !a_this->uiMovie) { return; }
				OnFrame(a_this->uiMovie.get());
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};
	}

	void Install()
	{
		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_JournalMenu[0] };
		AdvanceMovie::func = vtbl.write_vfunc(0x5, AdvanceMovie::thunk);
		logger::info("row: hook on JournalMenu::AdvanceMovie: vtable {:X}, slot 5", vtbl.address());
	}

	State GetState()
	{
		std::scoped_lock l(g_lock);
		return g_state;
	}
}
