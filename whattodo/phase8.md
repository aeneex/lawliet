# Phase 8: Tests and Docs

**Goal:** a small, meaningful test suite and one short README. No stale documents.

## Tests

Keep the custom runner (or simplify it if it's bloated). The suite should only cover what exists now:

- Mixer: mode II vs III composition, dry alignment, limiter ceiling, parameter clamping (phase 3).
- Settings: round trip, missing file, garbage, out-of-range, unknown keys, odd device names (phase 4).
- Pitch ratio sanity: the shifter at 0 semitones leaves a test tone's frequency unchanged, +12 doubles it, -12 halves it (within a small tolerance).

Delete every test that targets removed code. The expected count is whatever the suite now has; update any hard-coded "39" in scripts or docs.

## Docs

1. Delete `docs/` (including `ARCHITECTURE.md`) and `PROJECT_STATUS_REPORT.md`. Both describe the old project.
2. Write `README.md` (60 lines or fewer):
   - What it is: a Windows voice changer that layers a lower and a higher pitch copy of your voice, inspired by L from Death Note.
   - Requirements: Windows 10/11 and VB-Audio Virtual Cable (free, vb-audio.com/Cable).
   - Build: `build.bat`, or the two CMake preset commands.
   - Use: pick mic, set output to `CABLE Input`, pick `CABLE Output` as the mic in Discord/games. Modes II and III explained in one line each. Pitch ranges.
   - Discord settings: Krisp off, echo cancellation off, manual input sensitivity around -55 to -60 dB.
   - Monitoring: use headphones, otherwise you get feedback.
   - Settings: `lawliet.ini` next to the exe.
   - Licenses: SoundTouch (LGPL 2.1), Dear ImGui (MIT), GLFW (zlib), miniaudio (public domain / MIT-0). Point to the license files in `external/`.
   - Measured end-to-end latency, only if you actually measured it in phase 3 or 5.
3. Do not mention the old driver, APO, macOS, Linux, presets or JSON anywhere.

## Final sweep

- Repeat the phase 2 grep, extended with `preset|json|Equalizer|NoiseGate|VoiceEffects|ConfigManager|AppConfig`. Everything outside `external/` must be clean.
- Look for orphaned files: anything in `src/`, `include/`, `tests/` not referenced by CMake gets deleted.
- Debug and release builds pass, tests pass.

## Done when

- README exists, old docs are gone, tests pass, sweep is clean.
- `NOTES.md` has the summary you will give the user: backup path, deviations, measurements, before/after sizes.
