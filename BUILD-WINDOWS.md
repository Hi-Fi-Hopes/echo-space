# Echo Space — build and install on Windows

Phase 1: two strips (A and B), each running Digital, Tape, Room, Hall or Plate.
Routing is Series, Parallel or Split, with Freeze and Trails.

Total time the first time: about 30–45 minutes, mostly downloads and waiting.
After that, a rebuild takes 1–2 minutes.

---

## Part 1 — Install the build tools (one time only)

1. Go to **visualstudio.microsoft.com** and download **Visual Studio Community** (the free one).
   2022 is the safe choice. A newer version should also work.
2. Run the installer. When it shows the list of "Workloads", tick **Desktop development with C++**.
   Leave everything else as it is.
   This gives you the compiler, CMake and Ninja.
3. Click **Install**. It's a large download (around 7–10 GB). Restart the PC when it finishes.

You do **not** need Git or anything else. The build downloads JUCE (the plugin framework) by itself.

---

## Part 2 — Put the project somewhere simple

1. Make a folder: `C:\Dev`
2. Unzip `EchoSpace.zip` so you end up with `C:\Dev\EchoSpace\CMakeLists.txt`.
   - Check: open `C:\Dev\EchoSpace`. You should see `CMakeLists.txt`, `BUILD-WINDOWS.md`, and a `Source` folder.
   - Don't put it inside OneDrive, Desktop or Documents. OneDrive syncing can lock files mid-build.

---

## Part 3 — Build it

1. Press the **Windows key**, type **x64 Native Tools**, and open
   **"x64 Native Tools Command Prompt for VS 2022"** (or the matching name for your version).
   - It must be the **x64** one. The plain "Developer PowerShell" can build a 32-bit plugin, and Ableton won't load that.
2. In that black window, type these one line at a time, pressing Enter after each:

   ```
   cd /d C:\Dev\EchoSpace
   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   ```

   This step downloads JUCE (about 100 MB) and sets up the build. It takes a few minutes.
   It's done when the last line says **"Build files have been written to: C:/Dev/EchoSpace/build"**.

3. Then type:

   ```
   cmake --build build
   ```

   This compiles everything and takes 5–10 minutes the first time.
   It's done when the last line says something like **"[xxx/xxx] ... Echo Space.vst3"** with no word "error" above it.

If anything fails, copy the **last 30 lines** of the window and send them to me.

---

## Part 4 — Install the plugin

1. In File Explorer, go to:
   `C:\Dev\EchoSpace\build\EchoSpace_artefacts\Release\VST3\`
2. You'll see a folder called **`Echo Space.vst3`**. Copy that whole folder.
3. Paste it into:
   `C:\Program Files\Common Files\VST3\`
   Windows will ask for administrator permission. Click **Continue**.

---

## Part 5 — Load it in Ableton

1. Open Ableton. Go to **Options → Settings** (Ctrl + comma), then the **Plug-ins** tab.
   (Older versions call the menu item "Preferences".)
2. Turn **on** "Use VST3 Plug-in System Folders".
3. Hold **Alt** and click **Rescan**. Holding Alt forces a full rescan instead of a quick one.
4. In the browser on the left, click **Plug-ins → VST3 → DIY Audio → Echo Space**.
5. Drag it onto an audio track that has something playing: a drum loop or a guitar track.

### Quick check that it's working
- Default settings: **A = Tape** delay synced to 1/8 dotted, feeding **B = Hall** reverb.
- Turn **A Mix** up: you should hear dotted-eighth echoes that follow Ableton's tempo.
- Change Ableton's tempo: the echo time follows it.
- Click **FREEZE** while something is ringing: it holds indefinitely. Click again to release.

There's also a standalone app at `build\EchoSpace_artefacts\Release\Standalone\Echo Space.exe`.
It runs the plugin on its own with your audio interface, so you can test outside Ableton.

---

## Controls

**Main page (per strip)**

| Engine  | Time       | Repeats / Decay      | Tone              | Control 1 | Control 2         |
|---------|------------|----------------------|-------------------|-----------|-------------------|
| Digital | delay time | number of repeats    | darker / brighter | Spread (0 = normal stereo, 100 = ping-pong) | Crush (bit + sample-rate reduction) |
| Tape    | delay time | number of repeats    | darker / brighter | Age (darker, more saturated) | Wow/Flutter |
| Room    | pre-delay  | decay time (seconds) | damping           | Size      | Early reflections |
| Hall    | pre-delay  | decay time (seconds) | damping           | Size      | Mod (chorus-like shimmer in the tail) |
| Plate   | pre-delay  | decay time (seconds) | damping           | Diffusion | Mod               |

- **SYNC** (delay engines only): Time follows Ableton's tempo in note values (1/32 up to 1/1, with T = triplet and D = dotted).
- **ON**: turns the strip off. With **TRAILS** on, the echoes or reverb tail ring out naturally. With TRAILS off, they cut.
- **Double-click** any knob to reset it. **Click a value** under a knob to type a number in.

**EDIT page** (the EDIT button, top right): Low Cut and High Cut on the wet signal, stereo Width (up to 150%),
Mod Rate (wow speed for Tape, tail movement for Hall and Plate), and Duck (the wet signal dips while you play
and swells back when you stop).

**Routing**
- **Series**: A feeds into B (for example, delay into reverb).
- **Parallel**: both strips get the dry signal; their effects are added together.
- **Split**: left input goes through A, right input through B (dual mono).

Every control can be automated or mapped to an Ableton Rack macro.

---

## Rebuilding after a code change

1. **Close Ableton.** Windows locks the plugin file while Ableton has it open.
2. In the x64 Native Tools prompt:
   ```
   cd /d C:\Dev\EchoSpace
   cmake --build build
   ```
3. Copy `Echo Space.vst3` into `C:\Program Files\Common Files\VST3\` again, replacing the old one.

---

## Renaming the plugin

Open `CMakeLists.txt` in Notepad and change `PRODUCT_NAME` (the plugin name) and `COMPANY_NAME` (the folder
it shows under in Ableton). Delete the `build` folder, then repeat Part 3.

---

## If something goes wrong

| What you see | Fix |
|---|---|
| `'cmake' is not recognized` | You're in the wrong window. Use the **x64 Native Tools Command Prompt** from Part 3. |
| `No CMAKE_CXX_COMPILER could be found` | The C++ workload isn't installed. Rerun the Visual Studio Installer, click **Modify**, and tick **Desktop development with C++**. |
| Plugin doesn't appear in Ableton | Check that the `.vst3` folder is in `C:\Program Files\Common Files\VST3\`, the VST3 system folder setting is on, and you did an Alt+Rescan. |
| Ableton says the plugin failed to load | Most likely a 32-bit build. Delete the `build` folder and redo Part 3 from the **x64** prompt. |
| Build fails during download | Internet hiccup. Run the first `cmake -B build ...` line again. |
