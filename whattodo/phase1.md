# Phase 1: Baseline and Safety Backup

**Goal:** know exactly what you're starting from and keep a way back before anything is deleted.

## Steps

1. **Backup.** Copy the entire project folder, including `.git`, to a sibling folder named `<project>-backup-<yyyymmdd>`. Never write into it afterwards. Tell the user the path in your final report (they delete it whenever they want).
2. **Create** `refactor/NOTES.md`. Log everything below into it.
3. **Verify the tree.** Compare the real directory tree against section 7 of `PROJECT_STATUS_REPORT.md`. Note extra or missing files.
4. **Check git state.** Run and record the output of:
   - `git status --short`
   - `git remote -v`
   - `git config user.name` and `git config user.email`
   - whether `.gitmodules` exists, and whether any folder in `external/` contains a `.git` file or folder
   Do not commit, stash, or change anything in git.
5. **Find out how GLFW is obtained** (`external/`, `FetchContent`, `find_package`, system install). Log it. Phase 7 needs this.
6. **Baseline build.** `cmake --preset windows-debug`, then `cmake --build --preset windows-debug`. Run `lawliet_tests.exe` and confirm the report's `39 passed, 0 failed`. If it doesn't build or pass, log why but do not fix unrelated problems.
7. **Record baseline numbers** in `NOTES.md`: size of `lawliet.exe`, total line count of `src/` + `include/`, and the number of files under `external/`.
8. If you can run the GUI with audio on this machine, note the observed latency and CPU use. If not, skip.

## Done when

- Backup folder exists and its path is in `NOTES.md`.
- `NOTES.md` has the git state, GLFW source, baseline test result and size numbers.
- Nothing in the project itself has changed except the new `refactor/NOTES.md`.
