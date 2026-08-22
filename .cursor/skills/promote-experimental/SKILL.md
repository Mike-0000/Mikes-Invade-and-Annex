---
name: promote-experimental
description: Merges experimental into main for Mike's UI, HALO Jump, and Invade and Annex while keeping production Workshop GUIDs. Restores Workbench rdb dirt and resolves experimental/main conflicts in the Exp worktrees, then reruns, so one /promote-experimental finishes. Use when the user runs /promote-experimental or asks to promote experimental to production.
disable-model-invocation: true
---

# Promote experimental

One invocation. Do not stop for `resourceDatabase.rdb` dirt or experimental/main conflicts. Do not merge production by hand. Do not push. Do not edit `addon.gproj`.

## 1. Run the script

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "F:\Mikes-Invade-and-Annex\tools\Promote-Experimental.ps1"
```

The script restores `resourceDatabase.rdb` in production and `*-Exp` folders, then merges `experimental` into each production `main`.

## 2. If it fails

**Real uncommitted files** (anything except `resourceDatabase.rdb`): stop. Quote the paths. Do not stash, commit unrelated files, or skip identity checks.

**`resourceDatabase.rdb` still blocking** (old script): `git restore resourceDatabase.rdb` in that folder and rerun step 1.

**`CONFLICT_FILES:`** the script already `merge --abort`ed production. Recover in the matching `*-Exp` worktree — never `git checkout experimental` in a production folder.

| Production | Exp worktree |
|---|---|
| `F:\Mikes-UI` | `F:\Mikes-UI-Exp` |
| `F:\Mikes-HALO-Jump` | `F:\Mikes-HALO-Jump-Exp` |
| `F:\Mikes-Invade-and-Annex` | `F:\Mikes-Invade-and-Annex-Exp` |

For each conflicted addon:

1. `git restore resourceDatabase.rdb` if that file is dirty.
2. `git merge main` on `experimental`.
3. Resolve. Inspect hunks; do not take a whole-file `--theirs`.
   - `Scripts/Game/**`: keep experimental unless a hunk is a unique main-only fix.
   - `.cursor/rules/enfusion-enforce.mdc`: keep both facts and changelog lines.
   - `addon.gproj`: keep experimental (ours). Do not retarget GUIDs.
   - `resourceDatabase.rdb`: do not stage.
4. Commit the merge on `experimental` (why, not a file list).
5. Rerun step 1 **once**.

If the second run still conflicts, stop and quote `CONFLICT_FILES`. Do not start a third merge cycle.

Repos already promoted in the first run are fine (`Already up to date`).

## 3. On success

Confirm each production `addon.gproj`:

- Mike's UI: ID `MikesUI`, GUID `B3F91C6A4E275D08`
- HALO: ID `MikesHALOJump`, GUID `C4E8A27B1F906D53`
- I&A: ID `MikesInvadeandAnnex`, GUID `6556458885927F2F`

Tell the user the merge is done and they should publish from the **production** Workbench projects in order: UI, then HALO, then I&A. Do not publish from `*-Exp` folders.
