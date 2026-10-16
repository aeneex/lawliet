# Phase 6: Slim the GUI

**Goal:** one small window with only the controls the product needs. Keep the dark theme and the existing GLFW + OpenGL3 + ImGui setup.

## Window and loop

- Small fixed-size, non-resizable window (around 420 x 460, adjust to fit).
- `io.IniFilename = nullptr` so ImGui doesn't write `imgui.ini`.
- Keep idle cost low: vsync on, and wait for events with a timeout (`glfwWaitEventsTimeout(1.0 / 30.0)`) or cap at ~30 fps. The UI is static apart from a level meter.
- Release builds must not open a console window (see phase 7).

## Layout, top to bottom

1. **Mode:** two selectable buttons labelled **II** and **III**.
2. **Low pitch:** slider from -12 to 0, shown as `%.1f st`.
3. **High pitch:** slider from 0 to +12, shown as `%.1f st`.
   Use `ImGuiSliderFlags_AlwaysClamp`. Ctrl+click on an ImGui slider lets the user type an exact value, so add a one-line hint "Ctrl+click to type a value".
4. **Gains:** low, high, and dry (dry only enabled/visible in mode III), plus master. Put them under a collapsed header if the window feels crowded.
5. **Devices:** microphone combo, output combo, monitor checkbox + monitor device combo + monitor volume, and a small refresh button that re-enumerates devices.
6. **Input level meter** (keep the existing one if there is one, otherwise skip it).
7. **Status line:** engine status from phase 5.
8. **Hints:**
   - If `isCablePresent()` is false, show: `VB-Audio Virtual Cable not found. Install it from vb-audio.com/Cable and restart.`
   - Footer, two lines: `Discord: set input to "CABLE Output"` and `Turn Krisp and echo cancellation off`.

## Remove

Preset UI, EQ / effects / noise gate controls, "Lawliet Microphone" / native driver status, the long Discord guide, and any other panel not listed above.

## Behavior

- Every widget writes into the mixer/engine atomics immediately and updates a local `Settings` copy.
- Save `lawliet.ini` debounced (about 500 ms after the last change) and once on exit.
- Keep `PushStyleColor`/`PopStyleColor` (and every Push/Pop pair) balanced, since an earlier assertion crash came from that.

## Done when

- The window shows exactly the layout above and nothing else.
- Mode, pitch, gain and device changes take effect live and persist across restarts.
- Project builds and tests pass.
