# bcash tighten-up report — 2026-03-11

Scope: low-risk codebase tightening (no daemon start/restart, no consensus behavior changes).

## Actions completed
1. Ran full lint umbrella:
   - `test/lint/lint-all.sh`
2. Fixed whitespace issue in `src/pow.cpp` comment/function block.
3. Re-ran whitespace lint:
   - `test/lint/lint-whitespace.sh` -> PASS
4. Added BCASH client UX improvement in `src/bitcoin-cli.cpp`:
   - RPC connection errors now include target host:port and BCASH-specific guidance.
5. Added focused core gate script:
   - `test/lint/lint-core.sh`
   - Validated PASS locally.

## Current lint findings from lint-all
- Locale-dependence warnings in existing code (e.g., `atoi`, `stoul`, `to_lower`, `trim`).
- Legacy shebang/style findings in contrib scripts (python/python2 shebang expectations, shell locale export expectations).
- Shell locale checks in vendor/submodule areas.

These appear to be broader baseline lint debt, not introduced by this maintenance pass.

## Recommended next tightening steps (safe)
1. Create targeted lint allowlist policy for legacy/vendor scripts vs bcash-owned paths.
2. Add CI split:
   - `lint-core` (must pass)
   - `lint-legacy` (report-only)
3. Gradually modernize shebangs for bcash-owned Python scripts to `python3`.
4. Address locale-sensitive parsing in touched files as refactor opportunities.
