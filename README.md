# Echo Space

Dual delay / reverb VST3 for Ableton Live (Windows). Two channels (A and B), each running one of 15 engines.

## Install

1. Get `EchoSpace-VST3-Windows.zip` (from the `builds` branch of this repo, or from the latest run under the **Actions** tab).
2. Unzip it and copy the `Echo Space.vst3` folder into `C:\Program Files\Common Files\VST3\`.
   If an older version is there, close Ableton first, then replace it.
3. In Ableton: Settings → Plug-ins → turn on "Use VST3 Plug-in System Folders" → hold **Alt** and click **Rescan**.
4. Find it under Plug-ins → VST3 → DIY Audio → Echo Space.

Every push to `main` rebuilds the plugin automatically.

## Controls

Each channel has an LED, a footswitch (on/off), an engine menu, TAP and SYNC, then six knobs over six faders.

**Knobs:** Time · Repeats/Decay · Tone · Control 1 · Control 2 · Speed
**Faders:** Mix · Drive · Low Cut · High Cut · Width · Duck

- **Time**: delay time (or note value with SYNC on). For reverbs it's the pre-delay.
- **TAP**: tap the button in time (2 or more taps) to set the delay time. Tapping turns SYNC off; a 2-second pause starts a new count.
- **Repeats / Decay**: number of repeats, or reverb decay time in seconds.
- **Speed**: modulation rate. It's greyed out on engines that don't modulate.
- **Drive**: saturation going into the engine. The dry signal stays clean.
- **Duck**: the effect dips while you play and swells back when you stop.

| Engine  | Control 1 | Control 2 |
|---------|-----------|-----------|
| Digital | Spread (0 = stereo, 100 = ping-pong) | Crush |
| Tape    | Age | Wow |
| Analog  | Mod Depth | Grit |
| Oil Can | Wobble | Grit |
| Reverse | Smear | Octave |
| Decay   | Erode (each repeat wears more) | Dropouts (holes and crackle) |
| Dual    | Ratio (2nd delay x1/4 … x2) | Spread |
| Pattern | Pattern (Quarters, Triplets, Dotted, Gallop, Push, Rush, Swing, Late) | Spread |
| Room    | Size | Early reflections |
| Hall    | Size | Mod |
| Plate   | Diffusion | Mod |
| Spring  | Drip | Tension |
| Shimmer | Shimmer amount | Interval (+5th, +Oct, +Oct+5th) |
| Swell   | Attack (10 ms … 2 s fade-in per note) | Mod |
| Dome    | Size | Mod |

**Global**
- **Routing**: Series (A into B), Parallel, or Split (left input → A, right input → B).
- **Freeze**: holds whatever is ringing.
- **Trails**: lets echoes and tails ring out when a channel is switched off.
- **IN / OUT**: input and output level.

Tips: double-click any control to reset it, and click a value to type a number.

## Building it yourself (optional)

See `BUILD-WINDOWS.md`. You only need this if you want to compile on your own PC instead of using the GitHub build.
