# Phase 5: Simplify the Audio Engine and VB-Cable Routing

**Goal:** the engine does exactly this: capture mic, process, write to the cable output device, optionally also write to a monitor device.

```text
mic -> capture callback -> mono downmix -> mixer -> cable ring buffer   -> cable playback callback  -> CABLE Input
                                                 -> monitor ring buffer -> monitor playback callback (optional)
```

## Steps

1. **Strip the engine** down to three devices: capture, cable output, optional monitor. Remove everything left over from the removed virtual-mic backend and the removed DSP blocks (RMS metering may stay if the GUI meter uses it).
2. **WASAPI only.** Create the miniaudio context with the WASAPI backend only. Shared mode, float32.
3. **Device listing.** Expose `listCaptureDevices()` and `listPlaybackDevices()` returning `{name, id}`. Enumerate at startup and on demand (the GUI gets a refresh action later).
4. **Capture filter.** Keep hiding virtual endpoints (names containing `CABLE`, `VB-Audio`, case-insensitive) from the microphone list. Selecting them as the mic produces silence.
5. **Default device choice:**
   - Mic: saved device by name if it still exists, else the system default capture device (never a CABLE endpoint).
   - Output: saved device by name if it exists, else a playback device whose name contains `CABLE Input`, else the system default playback device.
   - Monitor: off by default.
6. **`isCablePresent()`**: true if a playback device with `CABLE Input` in its name exists. The GUI uses it for the install hint.
7. **Keep the proven fixes**, don't rewrite them: the 50 ms teardown grace and 4-attempt retry on device start, and the 1024-frame jitter cushion in the cable callback.
8. **Engine status** as a small enum plus message (`Running`, `Stopped`, `Error` + text) readable from the GUI thread via atomics or a mutex-guarded string that the audio thread never touches.
9. **Changing devices** from the GUI: stop, wait for the grace period, start again with retries. This runs on the GUI/control thread only.
10. **Sample rate:** keep what works today. Let miniaudio resample if a device doesn't match.
11. Keep existing start/stop behavior. Do not add new transport controls.

## Audio-thread hygiene check

Read all three callbacks. There must be no `new`/`malloc`, `std::string`, `std::vector` growth, mutexes, `printf`/logging or file I/O. Fix any that exist.

## Done when

- Engine has no references to removed features.
- Audio thread is clean per the check above.
- Project builds and remaining tests pass. If you can run the app with audio, confirm mic to `CABLE Input` still works and record the result in `NOTES.md`.
