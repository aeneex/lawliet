# Phase 4: Replace Config/JSON with an INI Settings File

**Goal:** a tiny, robust settings layer instead of a custom JSON parser, presets and schema validation.

## Delete

- `AppConfig.*`, `ConfigManager.*` (custom JSON parser/formatter, preset factories)
- `config/` folder (`config/presets/lawliet.json`)
- `test_config.cpp`, `test_preset.cpp`
- Preset UI hooks and any preset references in the engine and GUI

## Add `config/Settings.hpp/.cpp`

A plain struct plus load/save.

```cpp
struct Settings {
    int   mode = 2;               // 2 or 3
    float lowSemitones  = -6.0f;
    float highSemitones =  5.0f;
    float lowGain = 0.75f, highGain = 0.40f, dryGain = 0.50f, masterGain = 1.0f;
    std::string inputDevice, outputDevice, monitorDevice;
    bool  monitorEnabled = false;
    float monitorVolume  = 0.8f;
};
bool loadSettings(const std::filesystem::path&, Settings&);
bool saveSettings(const std::filesystem::path&, const Settings&);
```

## Rules

- File: `lawliet.ini` next to the exe. Resolve the exe directory with `GetModuleFileNameW`, not the working directory.
- Format: one `key=value` per line. Keys: `mode`, `low_semitones`, `high_semitones`, `low_gain`, `high_gain`, `dry_gain`, `master_gain`, `input_device`, `output_device`, `monitor_enabled`, `monitor_device`, `monitor_volume`.
- Device values are UTF-8 names: everything after the first `=`, trimmed. Devices are matched by name at startup.
- Ignore blank lines, `;`/`#` comments and unknown keys. Invalid or out-of-range values fall back to the default and are clamped to the mixer ranges from phase 3. Never throw.
- Missing file means defaults, and the file is created on first save.
- Save atomically: write `lawliet.ini.tmp`, then `MoveFileExW` with `MOVEFILE_REPLACE_EXISTING`.
- Keep the whole parser around 60 lines.

## Wire it up

- `main.cpp` loads settings at startup and passes them to the mixer and engine.
- The GUI keeps a `Settings` copy and saves it (debounced) in phase 6. Don't build GUI saving here.

## Tests (`test_settings.cpp`)

Round trip, missing file, garbage file, out-of-range values, unknown keys, device names with spaces and non-ASCII characters.

## Done when

- No JSON code left. No `config/presets`.
- Settings tests pass, project builds.
