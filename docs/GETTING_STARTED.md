# Getting started

## Requirements and downloads

- A homebrew-enabled PS4 with a working homebrew PKG installer.
- USB storage containing supported music.
- A HYPNOTIC package from [Releases](https://github.com/viktor23423512/HYPNOTIC/releases), once a public release is available.

No public PKG has been uploaded yet. Build 0.75 is a development build. Older firmware/GoldHEN support remains unverified; a successful compile is not a compatibility guarantee.

## Music on USB

Connect your drive before opening HYPNOTIC. Music can be in the drive root or nested folders; a folder called Music is a convenient starting point:

```text
USB drive/
├── Music/
│   ├── Album A/
│   │   ├── Track 01.flac
│   │   └── Track 02.flac
│   └── Song.mp3
└── HYPNOTIC/
    └── presets/
```

Open Music with OPTIONS. Left/right switches All Music and USB tabs; up/down selects an item. X opens a folder or plays a track. Circle goes back. Touchpad searches filenames across the USB library. L1/R1 pages through six rows.

Folders and drives without supported music are hidden. Scanning currently happens at startup, with a limit of 4,096 tracks, 1,024 folders, and 12 nested folder levels. Reopen the app after changing your music files.

## Supported audio

| Format | Support |
| --- | --- |
| WAV | PCM and IEEE float |
| MP3 | `.mp3` |
| FLAC | `.flac` |
| Ogg Vorbis | `.ogg` and `.oga` |

Supported inputs are mono or stereo at 8–48 kHz. Playback is converted to 48 kHz stereo. Supported higher-bit-depth WAV/FLAC audio is converted to 16-bit output. Ogg Opus, multichannel audio, DRM-protected files, chained Vorbis, and input rates above 48 kHz are not supported.

## Visualizers

Press Triangle to open Visualizers. Left/right changes tabs; up/down selects a preset. Browsing does not change the current visualizer: press X to apply it. R3 adds/removes a favorite; Touchpad searches. Favorites and Recent are saved across relaunch.

The AUTO button in the playback HUD opens a preset-cycle interval menu. Choose 5 seconds to 10 minutes and press X to apply. Cycling pauses while playback is paused or menus are open. Automatic changes keep a hidden HUD hidden.

## Importing MilkDrop presets

Put `.milk` files in `HYPNOTIC/presets` on your USB drive. The app also scans `/data/hypnotic/presets` on the console. Choose Imported and press Square to rescan.

Some presets need additional image textures. Keep the files supplied with a preset available to its texture lookup; texture requirements and shader support vary. If a preset is rejected, use the message to identify the missing feature or resource. Imported presets are not converted or rewritten individually.

HYPNOTIC's renderer supports a growing subset of MilkDrop. A preset loading successfully does not guarantee identical visuals or 60 FPS. Keep the original preset author's credits and respect their redistribution terms.

## Headset microphone test

Square in Music opens the microphone test. Connect the headset to a controller paired directly with the PS4 and confirm that the PS4 detects it under Audio Devices. Earbuds connected to a controller attached only to the PC running Remote Play are not automatically a PS4 microphone input.

Microphone support remains experimental. The test captures input for visualization; it does not record or play the microphone through speakers.

## Quality and performance

MilkDrop starts at 1080p by default unless a saved lower-quality setting overrides it. Heavy effects may run below 60 FPS. Built-in visualizers and MilkDrop use different rendering paths; their frame rates can differ.

[Controls](CONTROLS.md) · [Report a problem](https://github.com/viktor23423512/HYPNOTIC/issues/new/choose)
