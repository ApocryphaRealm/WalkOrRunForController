# MovementSpeedSlider - changelog

Written as changes happen, not reconstructed afterwards (rule 61). Each version carries its
**version-ledger status**, so this file cannot quietly claim more than the ledger does:

* **working** - observed running in game
* **untested** - built and packaged, not yet confirmed
* **failed** - built but crashed or malfunctioned; the number was reclaimed
* **scratch** - a hypothesis-test build that never held a real number

## 1.0.0 - 2026-11-04 - untested

### Added
- Default Motion, a second row beneath the slider: Run or Walk, default Run (nothing changes). On Walk the player walks with the real walk speed and animation, on a controller too; the Sprint button still sprints. Kept in the INI (bWalkByDefault), never in the save. msp.control op=motion.
- DevBench tool msp.control: the row's and the speed's live state, and set.
- Kept in the mod's INI (iSpeedPercent), never in the save: a hook on the player's read of SpeedMult. Holds across saves, new games and restarts.
- A slider among the game's own Gameplay settings (Journal, System, Settings, Gameplay, beneath Difficulty) that sets the player's movement speed, 25 to 150 percent, one percent per press, default 100. Scales walk, run and sprint alike, the player only.

### Known
- A mod that reads the player's current SpeedMult and writes a value back from it sees the scaled number. ModAV deltas are unaffected.
### Fixed
- On Walk, holding the Sprint button sprints; let go and you walk again. The game's own sprint test needs the player at or above a point between walk and run speed, so from a walk the flag it set was cleared the next frame (read from the 1.5.97 code, 0x6B8F20 and 0x5D1190). On Walk the mod's hook on the Sprint handler holds that flag and answers yes to is-running while the button is down. The handler is reached through the control map, so a button Unbind Vanilla Controls moved Sprint to works. On Run nothing changes.
### Changed
- Run and Walk keep separate speeds (the owner, 2026-11-05). The slider is named Run Speed or Walk Speed and shows and edits the selected Default Motion's value; switching brings the other's back at once. INI iRunSpeedPercent and iWalkSpeedPercent; an old iSpeedPercent starts both and its line goes at the next save. Sprint uses the Run value, also while Sprint is held on Walk. New text in eleven languages. DevBench: runPercent, walkPercent, appliedPercent, sprintHeld, sprintEvents.