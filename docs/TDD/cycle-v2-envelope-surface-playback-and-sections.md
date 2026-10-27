# Envelope surface playback and section colours

Status: Complete

The Envelope 3D display keeps Phase/time on x and the selected morph axis on y.
Replace the full-height Phase sweep with one diamond per voice at (envelope time,
resolved primary morph). Each diamond travels on that voice's horizontal cut line.
Keep the idle editor slice, the Envelope 2D Phase line, and ordinary Trimesh
cursors. Use a subdued 1px cut line and a hollow 8x8px diamond, with a light
outline over a dark stroke for contrast. Playback must look distinct from
editable circular intercepts.
No new interaction target or audio clock is required.

Use the existing flat bipolar material for pitch, so small deviations around
0.5 retain their sign. Use the flat Blues material for other Envelope surfaces, independent of the
preset's waveform material. Preserve its amplitude/brightness ramp while hue
shifting loop regions yellow and release regions red. Boundaries follow the
actual loop/sustain markers at each sampled morph slice, including time beyond
1 and morph-dependent boundaries. Leave samples outside each slice's first/last
authored intercept transparent; padding is not a release. Only shade release
when EnvelopePlaybackPolicy reports an actual tail after sustain. Pitch keeps
its bipolar hues across sections to preserve sign. Reuse EnvelopeMarkerPolicy at the existing
surface-rasterization boundary; do not rerasterize or scan meshes while painting.

EnvelopeMorphSurfacePreview owns sampled data and invalidation. A focused
EnvelopeSurfacePanel consumes a prepared colour image using Panel's existing
content texture and renderer lifecycle. This flat palette needs no new shader,
GPU resource owner, or changes to scalar height/relief infrastructure. Prepare
colour lookup tables once and use indexed lookup per sample; RGB preparation
and texture upload occur only on changed surface data. Cursor movement must
preserve graph, undo, audio-plan, surface preparation and texture-upload counts.
PanelPlaybackOverlay owns the shared line/dot geometry and matching automation.

Verify polyphonic time/morph pairing on all primary axes, loop/release positions,
2D and Trimesh preservation, per-slice marker extraction and colours, unchanged
surface caching during playback and gestures, zoom/clipping and context reload.
Build exact app/tests, inspect isolated production-size screenshots and MIDI
state, run architecture/code-map and whitespace/style checks, review file sizes,
then commit the complete slice without user preset modifications.


## Verification and size review

- Focused Cycle V2 tests: 1,863 assertions across 16 cases, including all three
  primary axes, independent polyphonic dots, 2D/Trimesh preservation, per-row
  colours, missing tails and small bipolar pitch deviations.
- Shared envelope surface tests: 31 assertions across four cases, including
  morph-dependent markers, actual release detection and absent-mesh coverage.
- The isolated 50-command surface fixture and its report verifier cover two
  staggered MIDI voices, continued motion, actual volume release positions >1,
  retirement, pitch/volume palettes, missing pitch release, surface zoom and
  a compact 1100x800 window. Surface builds and texture uploads remain at two
  throughout each editor's playback and zoom sequence. Document, undo,
  preview and audio-plan counters remain unchanged.
- The existing 27-command playback fixture passes for Envelope 2D/3D and
  ordinary Trimesh. Screenshots at 1728x962 and 1100x800 show readable dots,
  bipolar pitch, blue volume, real red release and background beyond the tail.
  UI reports and screenshots are under `.context/envelope-surface-*`.
- Debug/release standalone and test targets build. Architecture audit,
  code-map checker and whitespace checks pass; audit retains the repository's
  existing size-review warnings. Filtered isolated app logs have no new failures.

The new panel is 100 lines and owns only palette/image preparation plus the
existing content-texture adapter. Preview ownership remains cohesive at 326
lines. CurvePanelController grows from 871 to 873 lines: its existing base,
Trimesh and Envelope adapters coordinate panel lifecycle, model sync and
commands; these two new lines only forward polarity to the focused surface
owner. No rendering policy or second invalidation lifecycle is added there.
No generic renderer/resource lifecycle changed; context creation/destruction
continues through Panel/CommonGL's existing content texture. RGBA conversion
occurs once at that legacy texture boundary after JUCE resampling.

The old Envelope 3D full-height time sweep is deleted. The new geometry reuses
PanelPlaybackOverlay and panel transforms; audio clocks, scratch resolution,
ADSR looping and release remain with the established runtime owners.


Playback marker refinement: hollow diamonds replace circular dots, using the
existing renderer's line strips with stack-backed geometry. Debug/release builds,
the 50-command playback verifier and production-size screenshot review pass at
1728x962 and 1100x800. Marker centres, clipping and caching remain unchanged.
