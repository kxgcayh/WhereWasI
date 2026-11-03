# Where Was I

A small SKSE plugin for Skyrim players with a busy life outside the game.

You load a save after a week of work, and you are standing on a hillside with no idea why. Where were you going?
Which quest was it? Where Was I answers that once, as a short message when the save loads:

```
Last played 6 days ago
Quest: The Golden Claw - Retrieve the Golden Claw
Location: near Riverwood
Level 14 | 1,240 gold | Bounty 40
```

## Who this is for

This is for players who play in short sessions, days or weeks apart, and lose track between them.

If you play most days and remember where you left off, you do not need this mod. It only shows anything after a break
that is as long as you choose, so if you never take breaks it never appears.

## What it shows

| Line | Source |
|---|---|
| Last played | The real time since the save was written. Saves made before installing say nothing here. |
| Quest | Your active journal quest and its current objective. Names in the objective (people, items) are filled in. |
| Location | The named place you are in. In the wilderness it shows the nearest place you have discovered, such as `near Riverwood` or `2 km north-east of Whiterun`. It never names a place you have not found yet. |
| Level, gold, bounty | Your level, gold carried, and total bounty across all holds. Bounty appears only when you have one. |

The message appears once per load, after the loading screen. Click OK and play.

## Requirements

- Skyrim Special Edition or Anniversary Edition. Tested on 1.7.104 (AE). Built for 1.5.97 and 1.6.x too, but not tested on them.
- [SKSE64](https://skse.silverlock.org/) for your game version.
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444).

Skyrim VR is not supported.

## Install

Install the archive with your mod manager (Mod Organizer 2 or Vortex). The installer asks one question: how long a
break before the recap shows, in days (1 to 6) or weeks (1 to 4).

There is no ESP, so it does not use a plugin slot. You can install or remove it at any point in a playthrough.

## Settings

Your answer is saved in `Data/SKSE/Plugins/WhereWasI.ini`. In Mod Organizer 2 that file is in this mod's own folder,
under `SKSE/Plugins/WhereWasI.ini`. Changes take effect the next time the game starts.

```ini
[General]
fMinHoursAway=24   ; hours since the save was written: 24 = 1 day, 168 = 1 week, 0 = every load
bShowQuest=1       ; 1 = show the quest line, 0 = hide it
bShowLocation=1    ; location line
bShowSnapshot=1    ; level, gold and bounty line
```

## How it works

When you save, the plugin stores the real date and time in the SKSE co-save (the `.skse` file next to each save). When
you load, it compares that with the current time. If the gap is at least `fMinHoursAway`, it builds the recap from the
loaded game and shows it. A save made before installing has no stored time, so it always shows the recap once.

The plugin changes nothing in your game. It only reads what is already there and writes one timestamp to the co-save.

## Troubleshooting

The plugin writes a log to `Documents/My Games/Skyrim Special Edition/SKSE/WhereWasI.log`. The last five sessions are
kept as `WhereWasI.1.log` to `WhereWasI.5.log`. Each load logs the save name, the stored time, how long you were away,
and the full recap text. Include it with any bug report.

- **No message at all.** Check the log for `under threshold`: the save is newer than your `fMinHoursAway`.
- **Location says only `Skyrim`.** No place you have discovered is in this worldspace yet.

## Uninstall

Remove the mod. The timestamp left in your co-saves is ignored by the game and SKSE.

## Author

By Kautsar Bana. Original work; no assets or code from other mods. Built with
[CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG).
