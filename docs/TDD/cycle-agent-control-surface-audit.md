# Cycle Agent Control Surface Audit

## Intent

Cycle's agent automation should eventually support a broad "vehicle inspection"
style smoke pass: many small checks that prove the app can be navigated,
clicked, inspected, edited, saved, reopened, and rendered without relying on a
human moving the UI.

The current automation command set already has useful primitives:

- low-level pointer events: click, doubleClick, down, up, drag, move, wheel,
- generic `setControl` for named `Slider`, `Button`, and `ComboBox` targets,
- semantic actions through `CycleTour`,
- screenshots, assertions, mesh mutation, preset export/save/reopen, and audio
  capture.

The main coverage gap is target discovery and stable addressing. If a component
cannot be resolved by area and target name, the `pointer` and `setControl`
commands cannot reach it reliably. Menu items and popup choices are a separate
gap because they are transient `PopupMenu` rows, not stable child components.

## Current Named Surface

From `CycleAutomation.cpp`:

- 19 inspectable areas are listed in `kInspectableAreas`.
- 12 `TourGuide` areas are scanned for named targets in `kTourGuideAreas`.
- 96 named targets are listed in `kInspectableTargets`.
- 24 focused `scripts/fixtures/cycle-agent-*.json` fixtures exist.

Named coverage is strongest for:

- `MorphPanel`: morph sliders and link/range/prime buttons.
- `GeneralControls`: primary tool and view controls.
- `Waveform3D`: current time layer controls and mesh selection.
- Representative master/effect controls through `setControl`.
- Mesh inspection/mutation through dedicated mesh commands.

## Missing Command Families

These should be added before trying to claim near-total UI coverage.

1. Menu commands.
   - Add `listMenus` to return top-level menu names, item ids, text, enabled
     state, tick state, and submenu paths from `SynthMenuBarModel`.
     Done for main menu-bar `PopupMenu` trees.
   - Add `invokeMenuItem` with stable selectors such as
     `{ "menu": "Graphics", "path": ["Displayed Processing Stage", "3. After spectral filtering"] }`
     and an id fallback for tests that intentionally bind to enum ids.
     Done for main menu-bar items.
   - Use `SynthMenuBarModel::menuItemSelected(...)` for execution so tests hit
     the same handlers as JUCE menus. Done.

2. Popup commands.
   - Add domain-specific popup commands rather than trying to click native
     popup rows by pixels.
   - First targets:
     - `ModMatrixPanel` source, destination, and matrix-cell dimension menus.
     - `EnvelopeInter2D` layer config menu.
     - `SelectorPanel` layer selection menu.
     - `HoverSelector` and `MeshSelector` mesh menus.
   - Each command should expose inspect and invoke modes: list available items,
     then select by path/text/id.

3. Control surface registry commands.
   - Add a registry that can include ordinary components outside `CycleTour`.
   - Add stable target names for every button-like, slider-like, selector-like,
     and tab-like component that is currently only visible through
     `inspectTree`.
     Started for `MainPanel` top and bottom `TabbedSelector` controls, exposed
     as `AreaMain` / `TargMainTopTabs` and `TargMainBottomTabs`.
   - Keep `CycleTour` as a client of the registry, not the only registry.

4. Dialog commands.
   - Add explicit open/close/inspect commands for modal surfaces:
     - preset browser/page,
     - quality options,
     - file chooser wrappers where non-native mode allows it,
     - about/help,
     - modulation matrix.
   - Dialog state should include visibility, bounds, default button, focused
     component, and reachable controls.

5. Keyboard commands.
   - Add bounded key press/release/text commands for workflows that are
     keyboard-first or need focus behavior.
   - Keep preset-authoring state changes semantic where possible; use keyboard
     playback for regressions of actual focus/input handling.

## Static Coverage Gaps

### Knobs And Sliders

Generic `setControl` can set any named `Slider`, and `Knob` inherits
`Slider`. The remaining problem is naming every instance.

Currently named:

- master controls: volume, octave, duration,
- waveshaper: preamp, postamp,
- impulse: length, gain, highpass, zoom,
- Waveform3D/Spectrum3D layer pan/range representatives,
- morph sliders,
- vertex property sliders/gain knobs.

Needs target naming or domain commands:

- all `GuilessEffect` parameter knobs, especially delay, reverb, equalizer, and
  unison parameters that are only addressable by raw parameter index in the base
  effect class,
- hidden/conditional unison knobs that change visibility by mode,
- all mod-matrix utility sliders,
- all guide-curve and panel-control sliders when active area changes,
- every slider surfaced by `PanelControls::addSlider(...)` across layer groups.

### Buttons

Generic `setControl` can click named `Button` targets. Missing coverage again
is mostly naming.

Currently named:

- GeneralControls primary tool buttons,
- morph link/range/prime buttons,
- Waveform3D/Spectrum3D layer enable/add/remove and selected mode buttons,
- guide-curve add/remove through trigger actions,
- impulse load/unload/model wave buttons,
- playback zoom buttons,
- unison add/remove and mode controls,
- some effect enable paths through semantic `Enable`/`Disable`.

Needs target naming:

- all effect enable buttons, not only semantic enablement,
- all `LayerAddRemover` add/remove buttons as separate targets instead of only
  container targets,
- all `LayerUpDownMover` up/down buttons,
- `ModMatrixPanel` close/add-source/add-destination buttons,
- preset page previous/next/accept/cancel and search/filter controls,
- quality dialog apply/close and combo controls,
- file chooser wrapper buttons when non-native dialogs are used,
- tab selectors in `MainPanel` as named clickable targets.

### Popup Menus

No menu/popup inspection command currently exists. The following popup sources
need either semantic commands or a shared popup-model adapter:

- main menu bar in `SynthMenuBarModel`,
- mod matrix input/output/dimension popup menus,
- envelope layer config popup,
- layer selector popup,
- hover selector and mesh selector menus,
- mesh save/copy/paste/double actions in `MeshSelector`,
- tutorials submenu.

Main menu items that need coverage include File, Edit, Graphics, Audio, and
Help menus, including nested Graphics processing-stage, vertex-size,
detail-reduction, sample-display, Edit update/pitch-tracking, Audio
pitch-bend/oversample, and Help tutorials.

### Mouse Event Overrides

The `pointer` command already supports the event shapes needed by these
classes, but only if they can be resolved to named components or addressed by a
stable tree path.

High-priority custom mouse surfaces:

- `Interactor` and `Interactor2D`: mesh hover, selection, drag, wheel, and
  double-click behavior. Partially covered by mesh gesture commands.
- `Knob`, `HSlider`, `MorphSlider`, and `RatingSlider`: direct pointer
  interaction in addition to `setControl`.
- `IconButton` and `TwoStateButton`: click/drag applicability behavior.
- `TabbedSelector`: top/bottom/main tab changes.
- `SelectorPanel`, `LayerSelectorPanel`, `HoverSelector`, and `MeshSelector`:
  selection, wheel, popup, and drag behavior.
- `PlaybackPanel`: seek/drag behavior and zoom buttons.
- `MidiKeyboard`: key hover/click/drag.
- `MainPanel`, `BannerPanel`, `DerivativePanel`, `VertexPropertiesPanel`, and
  `ZoomPanel`: hover/click paths that currently do not have stable automation
  assertions.
- `SamplePlacer`: sample placement mouse workflow.
- `Dragger` and `PulloutComponent`: layout and pullout interactions.

## Preset Authoring Implications

Preset creation should not depend on pixel playback for core data changes.
Use this split:

- Semantic/direct commands for durable preset data: controls, effect
  parameters, layer properties, mesh edits, modulation mappings, sample refs,
  and preset metadata.
- Pointer/menu/keyboard commands for smoke tests that prove the real UI path
  still works.
- `exportPreset`, `savePreset`, and `openPreset` remain the verification
  boundary after a preset-authoring script.

Missing preset-authoring commands:

- create/reset from known empty preset,
- set preset metadata,
- add/remove/reorder layers by group with stable addressing,
- add/remove/reorder modulation matrix rows and mappings,
- set every effect parameter by stable effect/parameter id,
- select meshes/samples by stable library path,
- import/load sample in a deterministic fixture mode,
- verify all authored state through exported preset JSON.

## Proposed 99-Point Smoke Shape

The smoke should be a curated fixture suite, not literally one giant script.
Each check should have a short assertion and should leave the app in a known
state or reopen a fixture preset before continuing.

Suggested groups:

- startup, default preset, snapshot, assertion-path discovery,
- menu listing and a safe checked setting toggle per menu family,
- panel navigation and tab switching,
- all named buttons clickable,
- representative pointer click/drag/wheel for every custom mouse class,
- every slider/knob class set semantically and dragged once by pointer,
- every popup family listed and one safe item selected,
- all major dialogs opened, inspected, and closed,
- mesh add/select/move/delete and selection-frame gestures,
- effect enable/disable and parameter mutation for every effect UI,
- modulation matrix source/destination/cell edits,
- preset save/export/reopen verification,
- offline audio capture and OpenGL screenshot capture.

## Implementation Order

1. Add menu inspection/invocation around `SynthMenuBarModel`.
2. Introduce a shared automation registry that can name components outside
   `CycleTour`.
3. Register missing buttons/sliders/tabs/selectors area by area, starting with
   always-visible controls.
4. Add popup-specific inspect/invoke commands for mod matrix, selectors, and
   envelope config.
5. Add fixture groups for each newly covered area.
6. Promote the broad smoke to `scripts/run_cycle_agent_smokes.sh` once the
   individual fixtures are stable.
