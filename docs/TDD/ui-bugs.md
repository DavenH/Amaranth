# UI Bug Notes

## Open: Envelope native smoke does not restore curve state on undo

Context:

- `python3 scripts/test_cycle_v2_native_edit_smoke.py envelope` now completes
  both finite audio captures, then fails at `envelope_sequence` line 987.
- After the curve-sharpness gesture and undo, the exported Envelope model state
  does not exactly equal the pre-gesture state.
- The failure is downstream of and independent from the linked-stereo audio fix.

Current status: open; compare the restored model fields and determine whether
the gesture commits an incomplete undo snapshot or the fixture includes
non-semantic state in its equality assertion.
