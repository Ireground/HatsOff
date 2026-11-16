# HatsOff

A hi-hat pattern sampler (VST3) for REAPER. Load a short sample, generate or draw a pattern, and drag the result into REAPER as a MIDI clip.

- **Sample:** drop a WAV, AIF, FLAC, MP3 or OGG file on the waveform. Click the waveform to hear it.
- **EMBED** (on by default): the sample is saved inside the project. Loading a new sample replaces the old one. With EMBED off, only the file path is saved.
- **RUN:** plays the pattern at the project tempo without starting REAPER. Off = follows REAPER's transport.
- **SEQ:** turns the internal sequencer on or off. MIDI notes coming into the plugin always play the sample.
- **GENERATE / CLEAR / UNDO** (press UNDO again to redo).
- **RANDOMIZE:** NOTES (moves the hits, keeps the count), VELOCITY, ALL.
- **DRAG:** drag the green button onto a REAPER track to drop the pattern as a MIDI clip. The pill next to it sets the clip length (one pattern, or 1/2/4/8 bars).
- **Groove:** steps (1-32), rate (1/4 to 1/32 with triplets), swing.
- **Generator:** density, velocity variation, rolls.
- **Sound:** tune, decay, volume. **CHOKE** makes each hit cut the previous one.
- **Output:** MIDI note for the pattern, MIDI OUT on/off.

## Mouse

| Where | Action | What it does |
|---|---|---|
| Step | Click | On / off |
| Step | Click and drag across | Turns several steps on (or off, when you start on a lit step) |
| Step | Drag up/down | Velocity (Shift = fine) |
| Step | Mouse wheel | Velocity |
| Step | Right-click | Roll x2 / x3 / x4 / off |
| Strip under a step | Click / right-click | Roll up / down |
| Slider | Drag, mouse wheel | Change (Shift = fine) |
| Slider | Double-click | Reset |

## Building on GitHub

Every push builds the plugin automatically (Actions tab, about 10 minutes).

1. Open the latest green run in the **Actions** tab and download **HatsOff-VST3-Windows**.
2. Unzip it and copy the `HatsOff.vst3` folder to `C:\Program Files\Common Files\VST3`.
3. In REAPER: Options > Preferences > Plug-ins > VST > **Re-scan**.

The `jsfx` folder holds the JSFX version and its "Pattern to MIDI item" script.
