# Prior art for Movement Speed Slider (searched 2026-11-04, rule 24)

Wanted: one slider among the game's own Gameplay settings (Journal, System, Settings, Gameplay) that sets the player's
movement speed, not stored in the save, put back at every load from the mod's own settings, working on a controller on
SE 1.5.97.

Searched Nexus (skyrimspecialedition, GraphQL `nameStemmed`, op MATCHES): movement speed, walk speed, speed slider, speed
multiplier, walk toggle, walk controller, controller walk, gamepad walk, analog walk, analogue walk, walking speed, run speed,
speed control, player speed, walk run, always walk, toggle walk, slow walk, speed mult, movement speed slider, gameplay
settings, settings menu slider, gameplay menu, walk on controller, walk gamepad, walk mode, move speed, speed adjust, speedmult,
walk key, walk button, slower walk, faster walk, speed fix. Descriptions read through the same API for 179215, 197408, 194815,
38471, 154251, 178005 and 156801. A web search on the same words added Scrolling Speed NG and Variable Movement.

Searched GitHub (`gh search repos`): skyrim walk controller, skyrim movement speed, skyrim walk speed, skyrim speed slider, skse
walk toggle gamepad, skyrim analog walk, skse movement speed, skyrim gameplay settings menu scaleform, skse speedmult. One hit:
okwinza/skyrim-walk-speed-tuner.

**Nothing found does this.** No mod found puts a row in the game's own Gameplay settings. The nearest:

| Mod | Where | What it does | Why it is not this |
|---|---|---|---|
| Walk Speed Tuner 1.5.5 | Nexus 179215, github okwinza/skyrim-walk-speed-tuner (MIT source; its 1.5.5+ DLLs GPL-3.0-or-later). Supports 1.5.97 | SKSE plugin, save-clean by the same mechanism chosen here: the player's SpeedMult read returns an adjusted value, the actor value is never written. Own JSON, optional SKSE Menu Framework page | **The closest, and the owner may want to look at it.** Walk gait ONLY: run and sprint are untouched, and walk has to be selected first, which a controller cannot do; that is his whole problem. Hotkeys are keyboard and mouse only; no controller control; no row in the game's menu |
| Walk Speed Tuner Unofficial Update | Nexus 197408 | The same mod for 1.7 | Same |
| Controller Default Walking 1 | Nexus 194815 | Controller walks by default; Sprint once runs, twice sprints, by adjusting controller input | Answers the same complaint another way: it gives a controller a real walk gait, with no speed setting and no slider. Not read further (its source was not sought); version support not checked |
| Controller Walk Run Toggle SSE 1.0 | Nexus 38471 (ports Nexus Skyrim 93819) | D-pad Right toggles walk and run on a controller, target speeds settable | Papyrus; writes SpeedMult through scripts, so it lives in the save, the thing he excluded. A toggle, not a slider |
| Scrolling Speed NG 2.0.2 / Scrolling Speed 2 / Continued | Nexus 154251, 70455, 20959 | Mouse wheel changes movement speed in steps | Mouse wheel; no controller; no game-menu row |
| Variable Movement | Nexus Skyrim 29743 | Shift plus wheel between walk and run | Keyboard; original Skyrim |
| Dynamic Speed Controller 1.5.8 | Nexus 156801, github DanjelPiDev/TES5-DynamicSpeedController (Apache-2.0) | Per-state speed and attack rules; "adjusts your SpeedMult" | Much larger; writes SpeedMult; tested on AE only by its author; own menu |
| Lower Speed Limit 1.2 | Nexus 178005 | Minimum effective speedmult | Says itself it does not work on 1.5.97; another purpose |
| Fixed-speed esp mods (NK's Faster Walk Speeds, Walk Faster, Simply Better Movement Speeds, I am not a slow walker, Sprint Speed Multiplier and others) | Nexus 46688, 43329, 130, 161784, 133904 | Change movement records or game settings | One fixed speed, and they apply to NPCs too or are plugins; no slider |
| Sprint Sneak Movement Speed Fix | Nexus 86631 | Bug fix for wrong speed after drawing or shouting | Other purpose, but its bug is a reminder that the engine holds on to a computed speed |

## Read for the mechanism

Own reading, 2026-11-04:

- Walk Speed Tuner's `src/WalkSpeedHook.cpp` (MIT): the vtable hook of `GetActorValue` on the player and the refresh done by
  nudging CarryWeight. Its README, and DSC credited there, for the refresh. This mod writes its own code; it is not a copy, and
  it does not use its walk-gait gate, its limits or its keys.
- The game's journal, four copies exported with FFDec: vanilla, SkyUI 5.2, Quest Journal Overhaul, Norden UI Black. See README's
  "How it works" for what was read.
- The game's code, from the 1.5.97 and 1.7.104 dumps (gamecode.py): the `OptionChange` handler and the three movement readers of
  SpeedMult.

Own project: Custom Difficulty UI 1.0.8 (`6. current wip mods\CustomDifficultyUI-SMF`) does not touch the journal at all (an AMF
page plus game settings: no Scaleform in its source). Unbind Vanilla Controls is the project precedent for a C++ hook of
JournalMenu::AdvanceMovie and for adding rows to a System page list without replacing the SWF. Norden UI Black replaces the
journal SWF; its SettingsOptionItem and OptionsList are the same classes as vanilla's, by diff.
