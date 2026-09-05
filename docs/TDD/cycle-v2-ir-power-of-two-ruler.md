# Cycle V2 IR Power-of-Two Ruler

Status: Implemented 2026-09-01

## Objective

Make the IR ruler and its OpenGL grid use exact power-of-two sample landmarks,
including both sample zero and the impulse-length endpoint. For a 256-sample
impulse the major ruler must read `0, 32, 64, ... 256`.

## Authoritative Implementations

- Cycle 1's `IrModellerUI` and Cycle V2's existing zoom-aware ruler both derive
  labels from the panel-owned `vertMajorLines` and position them through
  `Panel::sx`. That single grid/ruler transform remains authoritative.
- `CycleDsp::IrModel` owns the IR sample domain: sample zero is
  `irDomainPadding`, while the impulse endpoint is domain `1`.
- `Panel::updateBackground` owns the mature major/minor grid construction. With
  `backgroundTimeRelevant == false`, it divides the declared active horizontal
  domain into eight major and 64 minor intervals.

## Root Cause

`FlatCurvePanelBase` treats its one horizontal padding argument as symmetric.
That is correct for Guide and Waveshaper panels but not for IR: IR reserves
space only before sample zero and ends at domain `1`. The unintended right
padding shortens the grid domain, producing rounded labels such as
`0, 29, 58, 86...`; it also keeps the endpoint out of the major-line list.

## Design

Represent left, right, and vertical domain padding independently in the shared
flat-curve base. Guide and Waveshaper retain symmetric values. IR declares
`irDomainPadding` on the left, zero on the right, and zero vertically.

The corrected active width divides exactly into eight major intervals, so the
existing OpenGL grid produces power-of-two sample steps for every supported IR
length. The IR panel appends domain `1` to its OpenGL major-grid lines when the
endpoint is visible; the read-only landmarks expose that same line and the
editor paints the corresponding impulse-length label there. No editor-side
zoom transform or independent tick-spacing algorithm is introduced.

## Test-First Contract

1. The default 1024-sample editor exposes nine landmarks: `0, 128, ... 1024`.
2. Every ruler x coordinate equals its panel-owned OpenGL landmark x.
3. The first landmark maps from `irDomainPadding`; the final one maps from
   domain `1` to the panel right edge.
4. Existing zoom-aware alignment, label clamping, audio, and curve tests remain
   green.

## Negative Boundaries

- Do not compute ruler spacing independently in the editor.
- Do not change the IR sample domain, impulse length, rasterization, or audio.
- Do not change Guide or Waveshaper padding/zoom geometry.
- Do not add JUCE grid painting; the grid remains in the OpenGL panel.

## Completion Criteria

- Focused ruler assertions fail before and pass after the padding correction.
- A production IR fixture shows power-of-two labels and the final endpoint.
- Standalone Debug, applicable suites, `git diff --check`, style review,
  hot-loop review, and production-diff review pass.

## Implementation Evidence

- The flat-curve base now represents left, right, and vertical domain padding
  separately. Guide and Waveshaper preserve their symmetric geometry; IR uses
  `irDomainPadding`, zero, zero as required by its authoritative sample domain.
- The IR panel adds visible domain `1` to its OpenGL major lines. The editor
  continues consuming those panel-owned domain and transformed x values, and
  clamps the final label to the plot edge.
- The focused test failed first with eight landmarks, then passed with nine
  exact `0, 128, ... 1024` landmarks and the endpoint at the panel right edge.
- The production 256-sample fixture passes and visibly reports
  `0, 32, 64, ... 256` in
  `/private/tmp/cycle-v2-ir-power-of-two-ruler.png`. Existing property and
  wheel-zoom fixtures also pass.
