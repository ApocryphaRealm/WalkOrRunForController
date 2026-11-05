# Movement Speed Slider 1.0.0 - first test (for Main Agent)

Nothing here has run in game. Rule 20: a step passes on what is seen, not on a log line. A step not reached is reported as not reached.

**Setup.** Package: `7. current test builds/Skyrim - Movement Speed Slider/Movement Speed Slider 1.0.0 FOMOD`. Install it with `install-test-build.py`, Line-SE-AE16 on 1.5.97, into a new MO2 folder with a meta.ini. Njordlinger list, Norden UI Black's journal winning.
A controller connected, `bGamepadEnable=1`. A disposable save (rule 51), standing in open ground. `[General] iSpeedPercent=100` in the
installed INI. Tool: DevBench `POST /api/tool/msp.control` (`{}` for state, `{"op":"set","value":N}` to set). Log:
`My Games\Skyrim Special Edition\SKSE\MovementSpeedSlider.log`.

**Measuring speed.** Twice in a row call the tool (`movementSpeed` is sampled at the previous call), and read `player.getpos` x and y over 5
seconds of the left stick held fully forward. Compare units per second to the 100% figure from step 2.

| # | Do | EXPECTED |
|---|---|---|
| 1 | Reach the game; read the tool and the log | `hookInstalled` true. Log has `speed: hook on the player's GetActorValue`, `owner at player+B0`, and `row: hook on JournalMenu::AdvanceMovie`. No crash |
| 2 | At 100%, run 5 s forward; sprint 5 s; read the tool | Baseline figures noted. `speedMultApplied` equals `speedMultBase`; `hookCount` rising |
| 3 | Pause, Journal, System, Settings, Gameplay, all on the pad | A row **Movement Speed 100%** directly beneath Difficulty, a slider like Look Sensitivity's, in Norden's look. The other rows and their order are unchanged. Tool: `rowPresent` true, `rowIndex` = Difficulty's + 1, `rowClip` not -1. Screenshot it |
| 4 | D-pad to the row. Tap D-pad left once, three times, once more | The label goes 99, then 96, then 95: **one percent per tap**, with the thumb moving and the row's own sound. INI reads 95 after each change. Tool `percent` follows |
| 5 | Hold D-pad left to the bottom, hold right to the top | Stops at exactly 25% and exactly 150%; thumb at each end of its track, never past it |
| 6 | Set 50%; **without saving**, close the menus; run 5 s | Speed **at once** about half of step 2's, with no draw, sprint or reload needed first. **This is the moment that proves the refresh; the speed not changing until the next stance change is a FAIL of the refresh, and the row's fault is ruled out.** Note how the run looks at 50%: does the animation slow or become a walk, or do the feet slide? Screenshot or describe |
| 7 | Sprint 5 s at 50% | Sprint about half of step 2's sprint. Walk, run and sprint scale alike |
| 8 | Back to the row; scroll the list with the D-pad so the row leaves the visible rows, if the list scrolls, and come back. Then move Look Sensitivity one tap | Row still shows 50% and its thumb is in the same place. **Look Sensitivity moves in its own steps (0..20), not one percent.** Tool: its value in the game's INI changes as vanilla does |
| 9 | On the pad, press Y (Defaults) on the Gameplay list and confirm | Row goes to 100% with the rest. Set it back to 50% afterwards |
| 10 | Leave for Display and for Audio, then come back to Gameplay. Move a Display or Audio slider | Neither list has the row. Their sliders behave as before: no row's range left on a reused clip. Audio volume changes as usual |
| 11 | At 50%: an NPC beside you, walk with a follower, or watch a guard run | **NPCs unchanged** (compare against getpos on one if it is unclear). Mount a horse: horse speed unchanged |
| 12 | At 50%, make a save, named. Quit to desktop | INI still 50 |
| 13 | Launch, load that save, run 5 s | About half speed straight after the load, label shows 50%. Log: `game loaded: movement speed 50%` |
| 14 | From the main menu, New Game (or a start mod), run 5 s once in control | Still half speed; the row shows 50%. Log: `game started: movement speed 50%`. The value did not come from a save |
| 15 | **Mod off.** Quit. Untick the mod in MO2, launch, load step 12's save | Loads clean. No missing-plugin or corrupt-save warning. Speed equals step 2's figures in normal gait. `player.getav speedmult` reads 100 and `player.getav carryweight` reads what it read in step 2. **Then tick it again** |
| 16 | **Other journals.** On plain SkyUI (Norden off), and on the game's own journal if the profile allows: repeat steps 3, 4, 8 | The same row in that journal's look, and the same one-percent taps. A setup not reached is reported beside the verdict |
| 17 | 1.7.104 instance, Line-17: steps 1, 3, 4, 6 | The same |

**Report also:** `getav speedmult` at 50%. It should read 50 from the console, since it goes through the hook; that it reads 50 is how the
hook looks from a script and is the known limit in README. Report anything that reads a speed in the list, such as a mod whose script does
`SetAV` from `GetAV`.

**Also tell the owner** when the test is reached: Walk Speed Tuner (Nexus 179215) does part of this on the same mechanism, walk gait only and
mouse-driven; Controller Default Walking (Nexus 194815) answers the same complaint with a real walk gait on the pad. See `PRIOR-ART.md`.
