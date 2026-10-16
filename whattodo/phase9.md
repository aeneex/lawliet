# Phase 9: Wipe Git History, Fresh Init, First Commit

**Goal:** the debloated project becomes the very first and only commit of a brand-new repository. This phase is destructive and irreversible (the phase 1 backup is the only way back). The user explicitly asked for it.

## Preconditions (check all, otherwise stop and fix)

- Phases 1 to 8 are complete. Release build is clean and tests pass.
- `git config user.name` and `git config user.email` are set (from your phase 1 notes). If either is empty, **stop and tell the user**. Do not invent an identity.
- The backup folder from phase 1 exists.

## Steps

1. **Record** (for the final report) the output of `git remote -v` and the old commit count (`git rev-list --count HEAD`).
2. **Delete `refactor/`** entirely. After you've read this file, it must not be in the first commit.
3. **Delete all git artifacts** from the project, including nested ones:
   ```powershell
   Remove-Item -Recurse -Force .git
   Remove-Item -Force -ErrorAction SilentlyContinue .gitignore, .gitattributes, .gitmodules
   Remove-Item -Recurse -Force -ErrorAction SilentlyContinue .github
   Get-ChildItem -Recurse -Force -Filter .git | Remove-Item -Recurse -Force
   Get-ChildItem -Recurse -Force -Filter .gitkeep | Remove-Item -Force
   ```
   Then confirm nothing is left: `Get-ChildItem -Recurse -Force -Filter ".git*"` must return nothing.
4. **Write a fresh `.gitignore`:**
   ```gitignore
   build/
   out/
   *.exe
   *.o
   *.obj
   *.log
   lawliet.ini
   lawliet.ini.tmp
   CMakeUserPresets.json
   compile_commands.json
   .vs/
   .vscode/
   .idea/
   ```
   Do not add a `.gitattributes`. Do not change any git config.
5. **Initialize:**
   ```powershell
   git init -b main
   git add -A
   git status --short
   ```
6. **Review what will be committed.** List `git ls-files`. Make sure there is no `build/`, no binaries, no `lawliet.ini`, no backup folder, no `PROJECT_STATUS_REPORT.md`, no `refactor/`. Flag any tracked file over 1 MB and confirm it is genuinely needed.
7. **Commit once:**
   ```powershell
   git commit -m "Initial commit"
   ```
8. **Verify:**
   - `git log --oneline` shows exactly one commit.
   - `git status` is clean.
   - `git branch` shows only `main`. `git remote -v` is empty. No tags.
9. **Fresh-clone build check.** Prove the repo is self-contained:
   ```powershell
   git clone . "$env:TEMP\lawliet-verify"
   cd "$env:TEMP\lawliet-verify"
   cmake --preset windows-release
   cmake --build --preset windows-release
   ```
   Confirm `lawliet.exe` builds and the tests pass if they're enabled, then delete the temp clone and return to the project folder.

## Do not

- Add a remote, push, tag, or create other branches.
- Run `git gc`, `filter-repo`, or anything else on the new repo.
- Modify or delete the backup folder.

## Final report to the user

Keep it short:

- Backup folder path (they can delete it once happy).
- Old remote URL(s) and old commit count, if there were any. The new history is unrelated to the old one, so to publish they run these themselves:
  ```powershell
  git remote add origin <url>
  git push --force -u origin main
  ```
  You must not run them.
- Before/after exe size and file counts.
- Measured latency, if measured.
- Any deviations from these instructions that were logged in `NOTES.md` (copy the relevant ones, since `NOTES.md` is deleted).
