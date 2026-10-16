# Phase 3: Slim the DSP, Implement Modes II and III

**Goal:** the DSP is only: pitch layers, mixer, limiter. Modes II and III work and sound clean.

## Delete

`Equalizer`, `VoiceEffects` (ring mod / warmth), `NoiseGate`: headers, sources, tests, CMake entries, and every reference in config fields, engine code and GUI code. The noise gate is off by default and not part of the product.

## DualLayerMixer

Rework it around three layers and two modes.

- Layers: **low** (SoundTouch shifter), **high** (SoundTouch shifter), **dry** (no shifter).
- `enum class Mode { II = 2, III = 3 }`:
  - II: `low * lowGain + high * highGain`
  - III: `low * lowGain + dry * dryGain + high * highGain`
- Parameters, all `std::atomic<float>` (mode as `std::atomic<int>`), written by the GUI, read once per block on the audio thread:

| Param | Default | Range |
| :--- | :--- | :--- |
| lowSemitones | -6.0 | -12 to 0 |
| highSemitones | +5.0 | 0 to +12 |
| lowGain | 0.75 | 0 to 1 |
| highGain | 0.40 | 0 to 1 |
| dryGain | 0.50 | 0 to 1 |
| masterGain | 1.0 | 0 to 2 |
| mode | II | II / III |

Clamp every input to its range inside the mixer.

## Latency alignment (the important part)

SoundTouch delays its output. The dry layer has no delay, so mixing it unaligned causes comb filtering and a phasey sound in mode III.

1. Add a preallocated ring-buffer delay line for the dry path.
2. Start from `getSetting(SETTING_INITIAL_LATENCY)` as the delay, then **verify empirically** in a test: feed a short impulse through a shifter at 0 semitones, find the output peak position, compare. If they disagree, use the measured value.
3. The low and high shifters must use identical SoundTouch settings so they stay aligned with each other.

## Variable output counts

`receiveSamples` returns a varying number of frames per call. Give each layer a fixed-capacity output FIFO and have the mixer pull exactly N frames per callback. If a layer is short, fill the remainder with silence and bump an atomic underrun counter. Never block, never allocate. Check how the current code handles this and fix it if it doesn't.

## Parameter changes

- Call `setPitchSemiTones` only when the target moved by more than ~0.01 semitone, from the audio thread, once per block.
- Smooth gains per sample (short linear or one-pole ramp, ~10 ms) so slider drags don't click.

## Latency tuning

Tune `SETTING_SEQUENCE_MS`, `SETTING_SEEKWINDOW_MS`, `SETTING_OVERLAP_MS` (quick-seek on) for speech. Pick the smallest values that still sound clean. Log the chosen values and the resulting latency in `NOTES.md`. Do not end up worse than the baseline.

## Limiter

Keep one hard limiter (or soft clip) after master gain so output never exceeds ±1.0. Wherever it lives now is fine.

## Tests (update `test_dual_layer_mixer.cpp`)

- Mode II output contains no dry component; mode III does.
- Dry path alignment: an impulse at 0 semitones in mode III lines up with the shifted layers.
- Output never exceeds ±1.0 with extreme gains.
- Clamping works for out-of-range parameters.

## Done when

- No Equalizer / VoiceEffects / NoiseGate anywhere.
- Mixer tests above pass, project builds.
