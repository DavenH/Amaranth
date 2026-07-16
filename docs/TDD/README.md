# Technical Design Documents

TDDs define both product behavior and the architecture permitted to implement
it. Passing tests is not sufficient when the implementation violates the
reuse, ownership, or deletion boundaries in the design.

## Required Design Evidence

Before implementation, a nontrivial TDD must identify:

- the authoritative existing implementations and source locations;
- behavior reused unchanged;
- behavior that must be extracted into a shared core;
- permitted adapter responsibilities, limited to representation, events,
  ownership, and lifecycle translation;
- forbidden duplication and forbidden dependencies;
- expected production files and a rough size/complexity envelope;
- temporary compatibility code and an explicit deletion target;
- expected complexity of hot interaction, DSP, traversal, and publication
  paths.

If the design cannot reuse mature behavior without copying it, stop at that
boundary. Specify the shared-core extraction or request architectural
direction rather than implementing an approximation or local replacement.

## Acceptance Criteria

Include positive behavioral criteria and negative architectural criteria.
Examples of negative criteria include:

- generic flat-curve infrastructure contains no Envelope types or marker
  state;
- adapters contain no interaction, rasterization, topology, or DSP algorithms;
- a single-vertex drag mutates the retained vertex without collection rebuild,
  sort, or serialization on each mouse movement;
- node-family extension does not require editing a generic kind switch;
- Cycle 1 and Cycle 2 exercise the same shared behavioral core.

Tests should protect observable semantics, parity with mature behavior, and
architectural boundaries. Tests that only exercise constructors, getters,
scaffolding, or implementation accidents do not count as completion evidence.

## Implementation Review

Before changing a TDD to `Implemented`, report:

- production lines added and removed;
- largest changed production files;
- new type/kind branches;
- mature source reused or extracted;
- remaining compatibility code and deletion status;
- focused semantic tests and end-to-end or UI automation evidence;
- complexity of ordinary edits and commit/publication paths.

Unexpectedly large adapters or duplicated behavior require design review. Do
not normalize them as implementation detail.

## Status Rules

- `Proposed`: design is not yet being implemented.
- `In Progress`: useful slices exist, but architectural or behavioral
  completion criteria remain.
- `Implemented`: all principal behavior, architecture, negative boundaries,
  and deletion targets are complete.

A compatibility façade may remain only when the TDD explicitly defines it as
the stable end state. Otherwise its continued existence prevents an
`Implemented` status.
