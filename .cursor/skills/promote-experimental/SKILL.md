---
name: promote-experimental
description: Merges experimental into main for Mike's UI, HALO Jump, and Invade and Annex while keeping production Workshop GUIDs. Use when the user runs /promote-experimental or asks to promote experimental to production.
disable-model-invocation: true
---

# Promote experimental

Merge `experimental` into `main` for the three addons by running the promote script. Do not merge by hand. Do not push. Do not edit `addon.gproj`.

## Steps

1. Run this exact command:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "F:\Mikes-Invade-and-Annex\tools\Promote-Experimental.ps1"
```

2. If it fails, stop. Quote the error. Do not stash, commit unrelated files, skip identity checks, or continue a half-finished merge.
3. If it succeeds, confirm each production `addon.gproj` still has:
   - Mike's UI: ID `MikesUI`, GUID `B3F91C6A4E275D08`
   - HALO: ID `MikesHALOJump`, GUID `C4E8A27B1F906D53`
   - I&A: ID `MikesInvadeandAnnex`, GUID `6556458885927F2F`
4. Tell the user the merge is done and they should publish from the **production** Workbench projects in order: UI, then HALO, then I&A. Do not publish from `*-Exp` folders.
