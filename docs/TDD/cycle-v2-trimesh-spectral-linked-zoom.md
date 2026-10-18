# Cycle V2 Trimesh spectral frequency zoom

Status: implemented (2026-10-18).

## Design

`LogRegionMapping` and Cycle 1 sampling remain authoritative: the first
harmonic is evaluated and positioned at display frequency 0.05. The editor
viewport alone starts at that coordinate for spectral magnitude, so the first
harmonic touches the 2D left edge and 3D bottom edge without changing presets
or audio. The compact preview uses the same default presentation crop, while
its sampled harmonic values remain unchanged.

`TrimeshPanelHosts` owns one frequency viewport shared by its two panels. Its
2D horizontal range `(x, w)` maps to the 3D vertical range
`(y = 1 - x - w, h = w)`. Each panel reports wheel and scrollbar changes to
that owner. Programmatic updates are guarded against callback loops; opening
the second panel inherits the current range. Magnitude gets the 0.05–1.0
default when entering its spectral range. User zoom survives routine node
refreshes and magnitude/phase switches. Other axes stay independent.

Zoom changes cost a constant amount of viewport bookkeeping plus the required
local panel redraw. They do not rebuild mesh samples or graph state. The old
independent frequency viewports are the deletion target, now removed from the
Trimesh panel host workflow.

The paired panel host owns viewport lifecycle and synchronization. The bridge
supplies domain facts and the default first-harmonic coordinate. The 2D and 3D
panels translate zoom events to the host; the compact renderer applies the same
default crop to its image. There is one synchronization policy and no new mesh
or DSP path. `TrimeshPanelHosts.cpp` grew from 300 to 435 lines and remains
below the architecture review threshold. The other touched production files
remain below 450 lines. No extraction is needed for this cohesive viewport
behavior.

## Verification

- Check the default 2D and 3D first-harmonic alignment and unchanged ramp.
- Change the 2D and 3D scrollbars and wheel zoom in both directions; check
  the matching inverse coordinate ranges and valid viewport bounds.
- Open either host after zooming the other; check that it inherits the range.
- Switch spectral magnitude, phase, and time profiles and refresh a node;
  check default reset versus preservation of user zoom.
- Inspect the focused `acid-stab-3` editor visually.
- Compare compact and expanded default band positions at the same preset state.

The focused viewport and compact-band CTests pass. The `acid-stab-3` visual
comparison reports matching compact and expanded bands, and the morph drag
capture retains stepped rows through release with no extra surface rebuild.
The linked-zoom test records unchanged slice and surface rebuild counters
through scrollbar and wheel movement.
The [expanded-editor capture](figures/cycle-v2-trimesh-spectral-linked-zoom.png)
shows the first band at the 3D bottom edge and the 2D frequency range beginning
at the first harmonic.
