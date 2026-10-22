# Cycle V2 Labelled Reference Studies

Status: Implemented

## Contract

Roadmap PR 6 adds direct reference import, catalogue inspection, numerical
analysis export and a host-side labelled study runner. Recordings remain owned
by the existing reference library, independently of presets. Study entries
reference library IDs; optional source paths import only when an entry has no
existing ID or byte-identical library recording. Observed and inferred labels
remain distinct, and omitted controls are unknown. Crops describe bounded observation intervals in the original
recording; they do not create trimmed audio or retrack pitch.

Automation commands are `listReferences`, `importReference` with `path`, and
`exportReferenceAnalysis` with optional `path` and `includeGrid`. Export reads
the selected reference and never changes selection; the study runner selects
each reference explicitly. Exported pitch paths/periods reflect current edits;
tracking insecurity and fallback flags describe original tracking confidence.
Cyclogram values are authoritative raw values, with their existing folding
gain and peak normalization documented. Absolute recording level is unavailable
after the existing decoder normalization and must not be inferred.

Schema 1's existing `effectiveMidiNote` and `pitchPath[].midiNote` fields retain
the Cycle sample convention (standard MIDI plus 12) for compatibility.
`midiNoteConvention` explicitly identifies that convention, while the additive
`standardMidiNote` and `pitchPath[].standardMidiNote` fields use standard MIDI.
`pitchFrames[].frequencyHz` remains the authoritative effective period frequency.

## Ownership And Reuse

- `ReferenceLibrary` owns import, prepared analysis, selected reference and
  pitch-edit persistence unchanged. `PitchedSample`, `PitchTracker` and
  `WavePeriodFolding` remain the only decode, tracking and folding algorithms.
- A small `ReferenceAutomation` serializer exposes library records and selected
  analysis; it performs no decode, analysis, UI publication or graph edit.
- `NodeWorkspace` routes import through the library and existing shelf reveal;
  `CycleV2Automation` translates three commands and resolves export paths.
- `scripts/cycle_reference_study.py` owns manifest validation, labels, crop
  ranges and study reports. It calls the existing coordinator and shared
  session client; no parallel transport, recording store or analysis cache.

Expected changes: two serializer files (~150 lines total), small automation
registration and workspace methods (<100 lines total), one host script
(~250 lines), focused C++/Python tests and an example study. Existing large
automation/workspace files retain lifecycle/command routing only; numerical
serialization stays in the focused reference owner. No compatibility bridge
or duplicate old path is introduced, so no production deletion is required.

## Lifecycle And Complexity

All commands run outside the audio callback through existing automation.
Catalogue work scales with library records; selected export scales with emitted
pitch/grid values. Grid values are opt-in. Import retains existing analysis
costs. The host validates the complete study before executing imports, bounds
case count and command timeout, requires an empty output directory, rejects
invalid crops rather than truncating them, and does not mutate the preset.
Existing cached analysis is reused. Source-only imports check byte identity
against same-size library files, with hashes cached only for the current run.

## Acceptance

- A study containing multiple references imports/selects them without a chooser,
  preserves labels/provenance, writes numerical analysis and can rerun by ID
  without adding duplicate recordings.
- Exported grids and pitch paths match the library before and after pitch edits;
  reload preserves corrections. Catalogue/export do not change graph state or
  reference selection. Missing selection and invalid imports fail clearly.
- Low-confidence tracking exposes insecurity and fallback explicitly; its pitch
  is never presented as a verified observation.
- Crops and label confidence reject nonfinite/out-of-range values. Missing
  labels remain missing. A crop beyond the recording fails without inventing
  shortened analysis or overwriting the source study.
- Focused semantic tests, actual app automation, architecture/size review,
  code-map validation and whitespace checks pass before completion.

## Evidence

The exact `CycleV2` and `CycleV2_tests` targets build in standalone-debug with
testing enabled. Eight focused reference/decoder cases and six host contracts
pass. Real coordinated app runs exported the factory sax-growl and sine grids,
then imported the noise fixture twice from the original source manifest into
separate study outputs. Both runs resolved one library record and identical
analysis hashes, preserved observed labels and low-confidence flags, and left
the exported graph unchanged. Artifacts are under
`.context/reference-study-first`, `.context/reference-import-first` and
`.context/reference-import-second`; build/test logs are
`.context/reference-build.log` and `.context/reference-tests.log`.

The size review found 119 lines across the two new serialization files and
236 lines in the host runner. Automation grows 1025 -> 1054 lines and workspace
933 -> 951; both retain narrow routing only. No DSP, NodeKind, algorithm or
realtime change is introduced. Production is approximately 400 added lines;
the full slice includes focused tests, documentation and a sample manifest.
The architecture audit retains the existing 24 size triggers. Code-map and
whitespace checks pass. Independent review identified repeated source imports
and unsafe reuse of an output directory; content matching and refusal of
nonempty output directories fix both, with focused regression coverage.
