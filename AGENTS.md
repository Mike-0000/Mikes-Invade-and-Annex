# Shared project rules (pi + Cursor)

Cursor's `.mdc` rule files are the single source of truth for this repository.
Pi automatically loads this file at startup; it must then read the referenced
rules, not treat these links as already-loaded rule contents.

## Always-on instructions

Before starting work, read these files completely and follow them throughout
this session (all paths are relative to this repository root):

- `.cursor/rules/experimental-worktrees.mdc`
- `.cursor/rules/commit-experimental.mdc`
- `.cursor/rules/enfusion-enforce.mdc`
- `.cursor/rules/mikes-ui.mdc`
- `Scripts/.cursor/rules/reforger.mdc`

Also discover `.cursor/rules/` directories throughout this repository and read
any additional `.mdc` rules with `alwaysApply: true`. For rules that are not
always-on, honor their declared scope (`globs`, description, or explicit user
invocation) before working on matching files/tasks.

After context compaction, re-read applicable rules if their full instructions
are no longer available. Re-read rules changed during the session before
continuing affected work. If a required rule cannot be read, report the problem
rather than silently proceeding without it.

## Keep one source of truth

Update the original Cursor rule when changing project guidance or recording
proven engine facts as required by the engine handbook. Do not copy rule bodies
here or maintain a separate pi-specific version. New always-on Cursor rules
are covered by the discovery instruction above.

Shell examples in the rules use PowerShell. Adapt their invocation to the
actual tool shell (pi's `bash` needs to invoke PowerShell explicitly), while
preserving the rules' branch, file-selection, commit, and no-push/no-promotion
constraints. Do not discard unrelated existing work when invoking helpers.
