# Controls

## Music and visualizer browsers

| Control | Music | Visualizers |
| --- | --- | --- |
| D-pad left/right | Change source tab | Change folder/category tab |
| D-pad up/down | Browse songs and folders | Browse presets |
| X | Open folder / play track | Apply highlighted preset |
| Circle | Back; close at source root | Leave search / close selector |
| L1/R1 | Page six rows | — |
| Touchpad left / L3 | Search all USB filenames | Search all visualizers |
| R3 | — | Add/remove favorite |
| Square | Mic Mode | Context controls: settings, rain toggle, or Imported rescan |

Hold the D-pad to repeat scrolling. Browsing never starts a track or preset until X is pressed.

In search: X types a character or activates Search; Square deletes; Triangle clears; Circle cancels; OPTIONS submits.

## Viewing a visualizer

- Right touchpad click opens Music; left touchpad click opens Search. Triangle opens Visualizers.
- OPTIONS opens Settings: appearance, playback/display preferences, Mic Test and credits.
- Remote Play without touch coordinates: touchpad click opens Music; L3 searches inside a browser.
- L1/R1 cycles built-ins or MilkDrop presets in the current category.
- L2/R2 scrubs through the current track; stronger trigger pressure moves faster.
- X pauses/resumes when Play/Pause is selected in the HUD. With another HUD button selected, X activates that button.
- While the HUD is visible, left/right selects Shuffle, Repeat, Previous, Play/Pause, Next, or AUTO.
- L3 pins/unpins the HUD. Normally it hides after five seconds without input during playback and stays visible while paused.
- Shuffle uses the current track's folder; Repeat cycles Off / All / One.
- AUTO opens the interval menu; X applies and Circle cancels.

## Built-in settings

- **Cymatics Sand:** Square opens Pattern, Sensitivity and Color. Patterns: Original, Ornate, Cells, Weave.
- **Cube Orbit EQ:** Square opens Rotation Speed, Direction, Circle Size, EQ Height, Cube Size, Breakaway Density, Flying Speed, Fragment Lifetime, Sensitivity and Color.
- **Matrix EQ:** Square opens its spectrum layout, vertical flip, height, character size, density, speed, fade, glow and demo settings.
- **Storm Sky:** Square toggles rain.
- **Oscilloscope:** Square opens Mode, Trail, Sensitivity and Color settings.

For Cymatics and Cube Orbit, Square also opens settings on the highlighted item in the visualizer selector. Up/down chooses a setting; left/right adjusts; Square/Circle returns. These settings currently last for the session.

For most other built-ins, up/down adjusts sensitivity and Square cycles colours. Some visualizers have their own context controls.

## Diagnostics

L3+R3 toggles debug mode with FPS and temperatures. For MilkDrop, debug begins with the reference render path for ten seconds and then switches to the optimized path using the same running preset state. R3 toggles the comparison manually. Normal playback starts with the optimized path immediately.

Square while viewing in debug selects 1080p or the original lower-resolution mode for the next launch. Relaunch to apply it. Disable debug before using the normal Square settings controls.

## Microphone

Square in Music switches to Mic Mode for visualization. Settings → Mic Test opens the input meter and diagnostic values. Normal microphone visualization hides microphone diagnostics. Input is captured for visualization without recording or speaker monitoring.
