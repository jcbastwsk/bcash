# bcash minor maintenance — 2026-03-11

Scope: low-risk housekeeping only (no chain restart, no daemon start).

## Current runtime snapshot
- node-0: bnetd stopped, explorer stopped, RPC 9332 closed, explorer 3000 closed
- node-1: bnetd stopped, explorer stopped, RPC 9332 closed, explorer 3000 closed
- node-2: bnetd stopped, explorer stopped, RPC 9332 closed, explorer 3000 closed

## Repo state (local)
- Branch: `master` tracking `origin/master`
- Large working tree with many modified files + untracked bcash-specific binaries/sources.
- No code modifications made in this maintenance pass.

## Safe next actions (manual GO required)
1. Confirm whether to keep chain data in `~/.bnet` intact.
2. If restart is desired, bring up node-0 first, verify RPC + block production, then node-1/2.
3. Keep explorer off until node consensus and peers are healthy.
4. After restart, run a 5-minute health sample:
   - peer counts
   - block heights aligned
   - mining status

## Notes
- This pass intentionally avoided touching consensus/difficulty code and avoided conflicting edits.
