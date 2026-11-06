# Walk or Run for Controller

Choose whether you walk or run by default, on a controller too, and set your speed with a slider. Both sit in the game's own
Gameplay settings.

A slider in the game's own menu (Journal, System, Settings, Gameplay, beneath Difficulty) that sets your movement speed, 25% to 150%,
one percent per press. It is kept in the mod's INI, never in your save, and holds across saves, new games and restarts, as the game's
own settings do. On a controller, where you cannot choose to walk, this is how you slow down.

- Scales walk, run and sprint alike. Default 100%: nothing changes.
- The player only. NPCs and your horse keep their speed.
- Beneath it, **Default Motion**: Run or Walk, default Run. On Walk you walk with the real walk animation and speed, on a controller too; holding the Sprint button sprints. Run and Walk keep separate speeds: the slider, named Run Speed or Walk Speed, shows the selected one's. Sprint uses the Run value. A horse walks too, as it does on a keyboard with run off.
- No plugin, no scripts. Works with any journal that keeps the game's System page: the game's own, SkyUI, Quest Journal Overhaul,
  Norden UI.

Requires SKSE and Address Library. Skyrim SE 1.5.97, AE 1.6.1170, and 1.7.99 / 1.7.104.

## How it works

**The row.** The System page builds its Gameplay list in ActionScript (`onSettingsCategoryPress` in `SystemPage.as`), draws rows with
`SettingsOptionItem.as`, and keeps the values in plain objects in the list's `EntriesA` (`OptionsList.as`, `BSScrollingList.as`). A C++
hook on `JournalMenu::AdvanceMovie` adds one more object to that array beneath `$Difficulty`, with the game's slider type (`movieType
0`). From then on the movie's own code does everything: draws it, moves it, writes the value into the object, and resets it on Y through
`defaultVal`. The mod reads the object back once a frame. Read in vanilla, SkyUI 5.2, Quest Journal Overhaul and Norden UI Black, whose
`OptionsList` and `SettingsOptionItem` match apart from SkyUI's mouse hit test and alpha values.

**The row's ID.** The movie also sends each change to the game's `OptionChange` with the row's ID. Read in the 1.5.97 and 1.7.104 code:
IDs above 0x1A (0x1B on 1.7) are looked up as forms and acted on only when they are sound categories. The row's ID, 0x7FFFFFF0, does
nothing there. 1.6.1170 has no code dump here; it sits between the two.

**The speed.** The game's movement code reads the player's SpeedMult through the player's ActorValueOwner vtable. The mod replaces that
slot on the player's own vtable and answers the game's value times the slider. Nothing is written to the actor value, so nothing reaches
the save. Set through the same read, walking, running and sprinting all scale. A brief nudge of the temporary CarryWeight modifier, up
and straight back, makes the game recompute the speed at once.

**Default Motion.** The game asks one small function whether to use its run or walk speed, and that function answers "the gamepad is enabled, or the run flag is on". That is why a controller cannot walk. The control map gives the pad no Run or Toggle Always Run key, and does not let them be bound. The mod replaces the function: on Run the same answer; on Walk with the gamepad enabled, no. Without a gamepad the run flag decides as usual, set off by the choice, so Shift and Caps Lock work as they always did. The start of the function is checked before it is replaced (1.5.97 id 41264, AE and 1.7 id 42343), and on a mismatch it is left alone with a line in the log.

**Known limit.** A mod that reads the player's current SpeedMult and writes a value back from it (a script that does `SetAV` from `GetAV`)
sees the scaled number, and could store it. A mod that only adds or takes away with `ModAV` is unaffected.

## Building

Visual Studio with C++, and `VCPKG_ROOT` pointing at a vcpkg checkout. `configure.bat` then `build.bat` for SE 1.5.97 and AE 1.6.1170;
`configure17.bat` then `build17.bat` for 1.7. `py -3 tools/make_translations.py` writes the eleven language files.

## Licence

GPL-3.0-or-later. See `NOTICE.md` and `THIRD_PARTY_NOTICES.md`.
