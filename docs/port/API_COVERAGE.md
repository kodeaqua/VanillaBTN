# Native SDK pending-call audit

Date: 2026-10-27. Scope: linked `port/psx/pending.c` traps and their source callers.
This is not a full-game completion claim or verification of every SDK semantic.

## Implemented in the runtime-fixes branch

- `ratan2`: signed 4096-unit turns, integer CORDIC, defined axes/zero and full
  signed input range. Used by camera, villagers, input and minigames. Tests compare
  a grid against mathematical atan2 within one angle unit. Exact original SDK
  last-bit parity has not been established.
- `csqrt`: integer square root with Q12 input/output scaling. Negative input is
  rejected rather than producing a fabricated value.
- Fifteen previously trapping `Mcx*` operations now complete through the existing
  asynchronous controller with NoCard or Invalid for ordinary memory cards.
  Request acceptance is distinct from successful device execution. No output
  buffer is populated with invented PocketStation data. Command identifiers other
  than the existing CardType and ExecFlag identifiers are internal opaque IDs;
  the audited source only compares ExecFlag. PocketStation emulation is absent.

## Eight remaining explicit failures

`FntFlush`, `FntLoad`, `FntOpen`, `KanjiFntClose`, `KanjiFntFlush`,
`KanjiFntOpen`, `KanjiFntPrint`, `SetDumpFnt` remain fail-fast debug-font APIs.
No SDK font data is bundled and font rendering is not replaced with fake success.

Most callers are DEBUG, PdaSamp and swind overlays. `FntPrint` is now implemented
in `psx/font.c` (ADR-082), outputting formatted text to standard error with
automatic flushing so that animation validation warnings, event-command
diagnostics, and overlay messages do not abort the process.
Full debug font primitive rendering on screen remains pending a font asset decision.

`psx.pending_inventory` rejects new pending names and calls from source files
outside this audited list. It does not detect a new call inside an already
listed file, dynamic calls or unrelated runtime errors. Runtime trig and card
tests cover the newly implemented calls without disc data.

Livestock transfer now uses a native partner view and bounded stash (ADR-066).
Other known gaps include original-PS1 save interchange, interactive transfer
failure/retry behavior, and full overnight/event/area coverage. Link success
and passing synthetic tests do not prove these paths safe.
