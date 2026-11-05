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