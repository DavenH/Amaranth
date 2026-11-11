# Compact performance bar

Status: Complete (2026-11-07).

Implement the approved dark-keyboard mockup in the existing Cycle 2 shell.
The canvas-wide 56 px band follows the left library boundary. Its occupied
controls are left aligned with 18 px inset: MIDI pattern heading and three
horizontal 28 px buttons, two-octave graphite keyboard with octave chevrons,
then 32 px radial MOD and mapped CC controls with label/value readouts.
The console starts below the band. Preserve sidebar, graph and reference tray.

PerformanceKeyboardPanel retains pattern transport, recording and MIDI routing;
JUCE Slider owns rotary relative drag, wheel, fine Shift movement and focus.
A 254 px ordinary drag covers 127 increments; keyboard arrows move one value,
Home/End reach endpoints. A viewport contains excess mapped controllers.
Controller movement keeps the existing ephemeral audition callback path, without
new graph copies, publication, or audio-thread allocation.

Extract key handling and panel chrome from the existing 1,001-line performance
file into cohesive key and chrome translation units. Keep playback scheduling
and recording in the panel implementation. Reuse AmaranthMidiKeyboard vector
key geometry and painting with a configurable palette; Cycle 1 defaults remain.
Delete vertical controller-track handling and stacked text transport layouts.

Verify key/CC output, relative drag/fine adjustment/endpoints, transport/record,
layout alignment/insets at normal and narrow widths, mapped-controller overflow,
and the exact app via automation plus native screenshots. Run architecture and
code-map checks, review sizes and production diff, and commit the complete slice.

## Ownership review

`PerformanceKeyboard.cpp` shrinks from 1,001 lines to under 600; it retains MIDI
routing, pattern scheduling and recording. `PerformanceKeyboardKeys.cpp` owns
key gestures/range selection and delegates vector rendering to the existing
Amaranth keyboard. `PerformanceKeyboardChrome.cpp` owns bar geometry, icon
painting and a narrow JUCE Slider specialization. The header stays below 300
lines. No new graph-edit or audio-thread behavior is introduced.

The bar aligns with the library boundary and has an intentional leading 18 px
inset; controls form one tightly spaced group and remaining width accommodates
mapped controllers. Keys have equal 6 px top/bottom insets and peer transport
buttons share a baseline. A viewport scrolls overflow controllers. Controller
count no longer enters canvas status geometry/repaint queries; the old count
helper, frame field and dock width calculation are deleted. Graph fitting clears
the console beneath the strip to prevent nodes being covered on initial load.

Shared keyboard defaults retain the original Cycle 1 palette. Only Cycle 2
supplies graphite/near-black keys and cyan active/selected keys. Record is a
neutral outline while idle and a red stop square while recording. The Edit
symbol depicts piano-roll note bars under a shared MIDI pattern heading.

## Verification results

The exact CycleV2 standalone app and test target build. Focused keyboard,
status and utility tests pass 670 assertions in 32 cases. They cover MIDI note
routing, playback/recording, no-jump relative CC dragging, Shift fine movement,
Home/End, one-value keyboard steps, overflowing mappings and repeated geometry.
The updated performance-keyboard app fixture passes all 41 commands. Its obsolete
pixel assertions now describe the compact bar; its release wait uses the supported
`idleDelayMs` field rather than the ignored `delayMs` spelling.

Native pointer checks pass for normal drag (40 to 80 over 80 px), Shift-drag
(40 to 44), MOD console help, Record/Play toggles, opening the MIDI pattern
editor, mapped CC73 output without document changes, and bar geometry at 800 px
and 1,728 px window widths. The transport test uses a ten-second authored pattern
so normal preview completion cannot race the second native click.

Artifacts: `.context/compact-topbar-tests.log`,
`.context/compact-topbar-fixture-report.json`,
`.context/compact-topbar-native-results.json`,
`.context/compact-topbar-final.png`, `.context/compact-topbar-narrow.png`.
The native reproduction is `.context/check_compact_topbar_native.py`.
Code-map, architecture audit, production diff/style review and whitespace checks
pass. The canvas still exceeds the size review threshold; this slice removes its
controller-count/frame bookkeeping and only changes utility geometry and fit
clearance. Broader canvas decomposition remains in `refactors.md`.

## Waveform follow-up

`cycle-v2-performance-waveform.md` replaces the progress stripe with a right-hand
cached Blues waveform on toolbar grey, uses real polyphonic playback cursors,
and gives the keys the recovered vertical space plus stable subtle grain.

## Three-octave grouping follow-up

Reorder the band to pattern transport, five CC slots, three-octave keyboard,
then the waveform. Anchor the keyboard immediately left of the grid-aligned
waveform with an 18 px gap after its octave-up button. Keep 24 px natural keys
at normal width (22 naturals including the terminal C); compact windows reduce
key width to 10 px while retaining all three octaves, hide the waveform first,
and scroll the controller viewport. Mapping changes reset its scroll position
so a sparse preset cannot leave MOD hidden behind empty slots. Spare space
separates pattern transport from the right-hand instrument group, rather than
splitting keys from waveform.

The controller strip paints muted, noninteractive outlines for unmapped slots;
real controls retain existing CC routing, precise relative dragging and overflow.
The keyboard owns its three-octave range, MIDI-boundary clamp and range reveal;
panel and automation query that same range. No new graph/audio behavior.
Verify final-note input, octave shifting and reveal at MIDI boundaries, stable
layout through preset mapping changes, placeholders and scrolling, compact
geometry, density ratios, and exact-app native captures. Build Debug/Release
and tests, run focused checks, review sizes/style/architecture, and commit.

Implementation stays with the existing panel/key/chrome owners. NodeWorkspace
remains 1,057 lines; its four changes replace hard-coded range limits with a
query to the keyboard owner, without adding responsibilities. The panel remains
under 600 lines, chrome 226, keys 266 and header 281. Three octaves still use the
shared Amaranth key geometry, texture and MIDI path. Only the placeholder strip
paints unused slots; it intercepts no mouse input and owns no controller values.

Completed and verified: exact Debug and Release apps build; 1,711 assertions in
37 focused keyboard, waveform and status cases pass. The 41-command performance
keyboard fixture passes with the three-octave range. The native waveform check
passes physical mouse down/up on the terminal key, MOD drag/release publication,
polyphonic cursors without cache rebuilds, edit/undo, and compact/normal resizing.
Geometry assertions cover tight key/waveform adjacency, fixed five-slot width,
mapping changes and overflow; rendered placeholders cannot emit MIDI. Key-edge
pixel checks cover the wider keyboard and fractional display scales.

Native captures in `.context/three-octave/` were reviewed at production size;
the waveform remains 768 physical pixels against 768 heat columns on Retina.
Evidence: `.context/three-octave-tests.log`, `.context/three-octave-fixture.log`,
`.context/three-octave-native.log`, and `.context/three-octave/results.json`.
Code-map, architecture audit, production style/diff and whitespace checks pass.
No new incidental runtime assertions or crashes were observed.
