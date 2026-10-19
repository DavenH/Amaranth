# Cycle Automation Coordinator TDD

## Status

Implemented. The coordinator, PID launcher, registry, leases, persistent
sessions, scoped runs, capture, video, LLDB, native-input routing, and product
aliases passed focused macOS integration acceptance.

## Problem

Cycle automation is safe when one caller owns one application instance, but
the host-side scripts do not establish that ownership. Independent Conductor
workspaces and terminal sessions can currently:

- launch Cycle applications with the same process name and bundle identifier;
- discover, focus, capture, or quit an instance by process name rather than PID;
- select the largest window belonging to a bundle identifier;
- share fixed `/tmp` socket, report, screenshot, and log paths;
- launch or focus GUI fixtures concurrently;
- test an app bundle that predates the source under investigation.

Observed failures include Cycle 2 fixtures launching Cycle 1, a partial app-path
override retaining Cycle 1 process metadata, native input reaching another
worktree's CycleV2 process, concurrent fixtures competing for one foreground
application, stale binaries producing misleading results, and macOS privacy
preflights being confused with command-execution approval.

Documentation and product-specific wrappers reduce accidental misuse, but they
cannot coordinate independent processes. The repository needs one host-side
authority for application identity, lifecycle, exclusive resources, and
artifacts.

## Goals

- Give every local workspace a stable automation identity.
- Make the product target explicit; never infer Cycle 1 when the caller means
  Cycle 2.
- Launch a specific app bundle and retain the exact resulting PID.
- Route focus, native input, capture, diagnostics, and shutdown by PID.
- Serialize resources that are genuinely machine-global while allowing safe
  semantic automation to run concurrently.
- Reuse one persistent automation session per workspace and product when
  startup behavior is not under test.
- Prevent stale builds from being mistaken for current behavior.
- Namespace sockets and artifacts by workspace, product, and run.
- Fail conflicts with enough ownership information to resolve them safely.
- Preserve the existing Cycle 1 and Cycle 2 command protocols and fixtures.

## Non-Goals

- Replacing `CycleAutomation` or `CycleV2Automation` command handling.
- Unifying the Cycle 1 and Cycle 2 command schemas.
- Making GUI automation available in a Conductor cloud workspace.
- Creating a unique bundle identifier or application name for every worktree.
- Automatically killing or taking over a live instance owned by another
  workspace.
- Treating longer sleeps, retries, or broad process-name cleanup as
  synchronization.
- Solving general multi-host or distributed test scheduling.

## Authoritative Existing Implementations

The coordinator is a host-side lifecycle and routing layer. It reuses these
implementations unchanged:

- `cycle/src/App/CycleAutomation.{h,cpp}` owns Cycle 1 commands, reports, and
  session transport.
- `cycle-v2/src/App/CycleV2Automation.{h,cpp}` and
  `CycleV2AutomationSessionTransport.{h,cpp}` own Cycle 2 commands and session
  transport.
- `scripts/cycle_agent_session.py` owns the newline-delimited JSON socket
  client behavior.
- `scripts/cycle_agent_smoke_session.py` owns fixture execution against a
  persistent session.
- `scripts/run_cycle_agent.sh` and `scripts/run_cycle_agent_session.sh` define
  the current artifact and command-line contracts during migration.
- `scripts/test_cycle_v2_native_edit_smoke.py` demonstrates PID-targeted focus
  and exact-executable process discovery.
- `scripts/capture_cycle_window.swift` owns ScreenCaptureKit window recording
  mechanics, but its bundle-id selection must be narrowed to PID.

No app-side UI, graph, DSP, or interaction behavior belongs in the
coordinator. Product wrappers may translate product choice into paths and
build targets; they must not duplicate lifecycle, locking, capture, or process
selection policy.

## Target Interface

Expose one command with explicit product and operation:

```sh
scripts/cycle_automation.py start --product cycle-v2
scripts/cycle_automation.py run --product cycle-v2 --fixture path/to/fixture.json
scripts/cycle_automation.py capture --product cycle-v2
scripts/cycle_automation.py status
scripts/cycle_automation.py stop --product cycle-v2 --owned
```

`--product cycle1|cycle-v2` is required for product-specific operations. The
command must not default to Cycle 1. Existing product-specific scripts remain
as thin compatibility entry points and delegate to this command.

The command prints a concise human summary and can emit structured JSON. Every
result includes the workspace identity, product, canonical app path,
executable fingerprint, PID, session socket, operation/run id, and artifact
directory when applicable.

## Identity And Paths

### Workspace Identity

Use `CONDUCTOR_WORKSPACE_ID` when it is present and
`CONDUCTOR_IS_LOCAL=1`. Outside Conductor, derive a stable identifier from the
canonical repository/worktree path. Do not use a branch name, workspace display
name, current directory, process name, or bundle identifier as identity.

Reject macOS GUI operations when `CONDUCTOR_IS_LOCAL=0` or the host is not
macOS. This does not prevent ordinary non-GUI builds and tests in cloud or
Linux environments.

### Coordinator State

Machine-shared state lives under a short, user-private directory such as:

```text
/private/tmp/amaranth-cycle-<uid>/
  registry.json
  registry.lock
  sockets/<short-workspace-id>/<product>.sock
```

Create the directory with mode `0700`. Keep socket components hashed and assert
the encoded path length before launch; macOS Unix-domain socket paths are too
short for arbitrary Conductor worktree paths. The registry lock protects short
atomic read/validate/write transactions. Use `fcntl` locking on macOS rather
than a check-then-create convention. Replace-write the registry. If it is
truncated or invalid, preserve it for diagnosis and make mutating commands fail
closed; do not discard ownership evidence and proceed as though no apps exist.

### Workspace Artifacts

Default artifacts live inside the calling worktree:

```text
.context/cycle-automation/<product>/<run-id>/
  report.json
  app.log
  app.log.raw
  screenshot.png
  capture.mp4
  crash.ips
  run.json
```

An explicit output path remains supported. Generated defaults must never use a
cross-workspace fixed socket or report path. Runtime sockets stay in the short
machine-shared directory; evidence stays in the worktree.

## Registry Model

An instance record contains at least:

- schema version;
- workspace id and canonical workspace path;
- product;
- canonical app and executable paths;
- executable fingerprint;
- PID and process start identity;
- bundle identifier as descriptive metadata, not routing identity;
- session socket and successful ping time;
- launch time and owning coordinator/run id;
- current operation and exclusive resource leases.

The executable fingerprint must detect a rebuilt bundle. Device/inode, size,
and nanosecond modification time are sufficient if they are captured after the
build; a content digest may be used when measurement shows it is cheap enough.

A PID is valid only when its current executable path and process-start identity
match the record. PID existence alone is not proof of ownership because PIDs
can be reused.

## Lease Model

Leases are logical records updated under the registry lock. Waiting is never
implicit. A conflicting command fails immediately by default and reports:

- the resource name;
- owning workspace id and path;
- owning PID and run id;
- operation and acquisition time;
- a `status` command that can inspect the owner.

An explicit bounded `--wait <seconds>` may wait for a lease. There is no
unbounded wait.

### Per-Instance Command Lease

Only one caller may mutate or drive a workspace/product instance at a time.
Semantic commands from different app instances may run concurrently after PID
identity is established.

### Lifecycle Lease

Application launch, restart, and shutdown take a short machine-wide lifecycle
lease. This prevents simultaneous LaunchServices/CoreMIDI startup and shutdown
contention. The lease ends after the exact process and session are known; it is
not held for the lifetime of an idle persistent instance.

### Foreground Lease

Native mouse/keyboard input, foreground activation, desktop-region capture,
and any operation depending on the global pointer or frontmost application
take one machine-wide foreground lease for their complete duration.

PID-targeted ScreenCaptureKit window capture and app-side component screenshots
may run without the foreground lease after tests prove that they neither focus
an app nor select by bundle id. Until that proof exists, they remain exclusive.

### Stale Leases

A lease is stale only after validating that both its coordinator owner and
recorded application identity are gone or invalid. Recovery removes registry
state and owned socket files only. It must not kill a process discovered during
recovery.

There is no automatic takeover of a valid live lease. A future manual recovery
command may stop a live owner only with explicit user intent and must print the
resolved PID, executable, and workspace before acting.

## Process Lifecycle

### Build And Freshness

`run` builds the exact standalone target incrementally by default:

- Cycle 1: `standalone-debug`'s Cycle application;
- Cycle 2: `standalone-debug --target CycleV2`.

An explicit `--no-build` is allowed. After building, compare the app
executable's fingerprint with the registered instance. If it changed, request
a clean quit through that instance's socket, wait for the exact PID, and launch
the new bundle. Never continue against an instance with a different
fingerprint while claiming to test the new build.
Hold a workspace/product build lease through the build, fingerprint check,
and instance selection. Fresh-fixture suites build inside each child run so a
concurrent caller cannot rebuild between the suite build and fixture launch.

### Launch

Use a small macOS helper around `NSWorkspace` to open the canonical app URL with
the automation arguments and return the resulting `NSRunningApplication` PID.
Do not infer the new process by subtracting process-name snapshots when a
direct PID can be obtained.

The coordinator validates the returned executable path, records its process
start identity, waits for the unique session socket, and verifies it with a
semantic `ping` before publishing the instance record.

### Persistent Session Reuse

Maintain at most one registered instance per workspace and product. Reuse it
for semantic fixtures and inspection when:

- the PID identity is valid;
- the socket answers `ping`;
- the executable fingerprint matches;
- the caller did not request startup-state coverage or `--clean`.

Commands for that instance are serialized through its per-instance lease. A
fixture that requires startup behavior launches a dedicated run-scoped instance
and does not replace the persistent record unless explicitly requested.

### Shutdown

The owner first sends `quit` over the recorded socket and waits for the exact
PID to exit. It may signal that PID after a bounded timeout. It must never use
`killall`, quit by process name, or target all applications sharing a bundle
identifier.

## Focus, Input, And Capture

- Focus through System Events using `first process whose unix id is <pid>`.
- Native input revalidates PID ownership immediately before acquiring target
  bounds and immediately before dispatch.
- ScreenCaptureKit helpers accept PID, and optionally a discovered window id;
  they do not select the largest window for a bundle id.
- Requested regions are cropped from the PID-selected ScreenCaptureKit window;
  desktop `screencapture` cannot establish which process supplied the pixels.
- App-side `screenshot` remains preferred for ordinary JUCE components.
- Every screenshot/video result records PID, window id where applicable,
  bounds, pixel dimensions, and capture method.

macOS Accessibility and Screen Recording preflights remain separate from
command-execution authorization. A failed AppleScript or LaunchServices call
must preserve its actual error and must not automatically be reported as a
privacy-permission failure.

## Conductor Integration

Conductor supplies `CONDUCTOR_WORKSPACE_ID`, `CONDUCTOR_WORKSPACE_PATH`, and
`CONDUCTOR_IS_LOCAL`; the coordinator consumes these but remains usable from a
normal terminal.

A repository `.conductor/settings.toml` may expose common coordinator commands
as local-only run actions. `scripts.run_mode = "nonconcurrent"` is an optional
secondary guard for those buttons, not the source of truth: agents and terminal
users can invoke repository scripts directly, so all safety remains inside the
coordinator.

Do not derive coordination from `CONDUCTOR_PORT`; Unix socket and artifact
paths are workspace-owned and do not need a network port.

## Failure Semantics

- Wrong or omitted product: fail before build or launch.
- Live lease conflict: fail with owner details; do not focus, quit, or delete.
- Stale registry record: recover only after PID/start/executable validation.
- Socket collision: fail unless the path is owned by the same valid record.
- App exits before socket readiness: report exact PID exit and attach the
  scoped log/crash artifact.
- Build changes the executable: restart or fail; never silently reuse it.
- Capture finds zero or multiple windows for the PID: fail with discovered
  window metadata instead of guessing.
- Privacy preflight failure: name the missing macOS permission and caller;
  never request broader shell execution privileges.

All failures preserve existing artifacts and leave unrelated instances
untouched.

## Expected Implementation Shape

Expected new or substantially changed production tooling:

- `scripts/cycle_automation.py`: CLI and high-level operation dispatch,
  approximately 150–250 lines.
- `scripts/cycle_automation_support/`: product profiles, registry/leases, process
  lifecycle, artifacts, and session client adapters; focused modules should
  remain below approximately 300 lines each.
- a Swift `NSWorkspace` launch helper returning PID, approximately 150 lines.
- PID support in `scripts/capture_cycle_window.swift`, without duplicating its
  ScreenCaptureKit recording implementation.
- thin delegation changes in existing Cycle 1/Cycle 2 run, session, capture,
  smoke, LLDB, and native-input wrappers.
- unit tests under `scripts/tests/` and focused macOS integration fixtures.

The coordinator must not become one large product/kind switchboard. Product
differences belong in declarative profiles: build preset/target, app-relative
path, expected executable, bundle id, and compatible fixture prefix.

## Migration Plan

### Slice 1: Identity, Registry, And Exclusive Safety

- Add workspace/product identity and artifact naming.
- Add registry validation and lifecycle/foreground leases.
- Add `status` with actionable conflict output.
- Route existing wrappers through the coordinator while retaining one-shot
  behavior.
- Conservatively hold the foreground lease for all current capture paths.

### Slice 2: Exact Process And Window Identity

- Add the `NSWorkspace` PID-returning launcher.
- Replace name-based focus, quit, and crash association.
- Make ScreenCaptureKit recording/capture select by PID.
- Convert native-input and UI-capture fixtures to exact PID.
- Prove that two same-bundle apps from different worktrees cannot cross-route.

### Slice 3: Persistent Workspace Sessions

- Reuse one healthy session per workspace/product.
- Serialize commands per instance.
- Restart on executable fingerprint change.
- Support explicit clean/startup-scoped runs.
- Move all default artifacts into `.context/cycle-automation/`.

### Slice 4: Safe Parallelism And Conductor Entry Points

- Demonstrate concurrent semantic fixtures against distinct instances.
- Retain exclusive foreground/native-input operations.
- Add optional local-only Conductor run actions with nonconcurrent mode where
  useful.
- Remove obsolete compatibility mechanics and update the runbook.

Each slice is committed only after refactor, style, focused tests, and its
deletion targets are satisfied.

## Deletion Targets And Stable End State

Delete after callers migrate:

- process-name `process_exists`, `process_frontmost`, focus, and quit helpers;
- bundle-id-only window selection;
- fixed default session sockets and report/log paths shared across worktrees;
- repeated `open -n`, PID discovery, wait, and cleanup implementations;
- product metadata duplicated across wrappers;
- manual `CYCLE_APP_PATH`/`CYCLE_PROCESS_NAME` recipes for ordinary use;
- broad cleanup that can terminate another workspace's app.

Stable compatibility wrappers may remain only as declarative product aliases
that `exec` the coordinator. They contain no process discovery, locking,
lifecycle, capture, or artifact policy.

The app-side automation implementations and command schemas are not deletion
targets.

## Verification

### Unit And Process Tests

- Product is mandatory and maps to the expected build target/app path.
- Workspace identity uses Conductor id when local and canonical-path fallback
  otherwise.
- Concurrent registry writers cannot both acquire the same lease.
- Separate workspace/product instance records do not share sockets or
  artifacts.
- A reused PID with a different process-start identity is rejected.
- Stale recovery does not signal any process.
- Live conflicts report their owner and make no launch/focus/quit calls.
- A changed executable fingerprint forces restart or explicit failure.
- Bounded wait succeeds after release and times out deterministically.
- Compatibility wrappers pass only product and user arguments.

Tests use fake process/session/launch adapters; they do not require a GUI for
ordinary registry and policy coverage.

### macOS Integration Tests

- Launch CycleV2 from two worktrees with the same bundle id and verify each
  socket command reaches its recorded PID only.
- Capture each instance by PID and prove the reported window/process identity
  matches its workspace.
- Hold the foreground lease in one process and prove another native-input run
  fails before focusing or clicking.
- Launch two lifecycle operations concurrently and prove they serialize
  without CoreMIDI/LaunchServices contention.
- Crash one registered app and prove stale recovery does not affect the other.
- Rebuild one target and prove its workspace instance restarts while the other
  remains untouched.
- Run existing focused Cycle 1 and Cycle 2 fixtures through compatibility
  wrappers.

### Required Evidence Before `Implemented`

- Before/after inventory of name-based and bundle-id-only decision sites.
- Before/after list of fixed cross-workspace `/tmp` defaults.
- Production line counts and largest coordinator modules.
- Demonstration artifacts from the two-worktree identity test.
- Operation log showing lifecycle and foreground lease acquisition/release.
- Existing Cycle 1 and Cycle 2 focused smoke results.
- Confirmation that every deletion target is gone or explicitly retained as a
  stable thin alias.

## Completion Criteria

- Every launch produces and records an exact validated PID.
- No automation path focuses, captures, quits, or signals an app solely by
  process name or bundle identifier.
- Two worktrees can keep same-product instances alive without cross-routing
  semantic commands or captures.
- Machine-global foreground and lifecycle work is serialized with fail-fast,
  owner-identifying conflicts.
- Repeated semantic work reuses a healthy workspace session.
- Default builds are current and executable changes cannot silently reuse a
  stale process.
- Default sockets and artifacts are workspace/product/run scoped.
- Existing wrappers are thin product aliases or deleted.
- Existing Cycle 1 and Cycle 2 focused fixtures pass through the coordinator.
- The runbook and semantic code map describe the coordinator as the canonical
  automation entry point.

## Implementation Evidence (2026-10-18)

- Before: the Cycle 1 run, session, capture, and LLDB scripts used process-name
  focus/quit or `open -n`; the window recorder selected the largest bundle-id
  window. After: `rg` finds none of these decision sites under `scripts/`.
- Before: runner defaults used fixed `/tmp/cycle-agent.sock`, report, and log
  paths. After: coordinator defaults use hashed workspace/product sockets and
  `.context/cycle-automation/<product>/<run-id>/`; fixture-authored absolute
  output paths remain explicit inputs.
- Coordinator production sizes: CLI 272 lines; largest support module 265
  lines. The Swift launcher is 45 lines, screenshot helper 56, recorder 124.
- Focused unit and alias tests: 14 passed. Both exact standalone targets built;
  Cycle 1 and Cycle 2 readonly fixtures passed through aliases. Cycle 1 focused
  session smoke passed with an empty reset preset.
- A detached second Git worktree with a copied Cycle 1 bundle produced PID
  53045, socket `469d273d9e5a/cycle1.sock`, and window 127598. The main
  worktree produced PID 53236, socket `b04bd5a7357a/cycle1.sock`, and window
  127608. Both semantic socket runs and PID-window captures succeeded. Evidence
  is saved in `.context/automation-two-worktree-evidence.json` with two PNGs.
- Foreground lease conflict failed before capture with the owning workspace,
  PID, run, operation, and status command. Lease acquisition/release events
  are recorded in the user-private shared `events.jsonl`.
- Rebuilding Cycle 1 restarted only its workspace instance; another registered
  instance retained its PID. A `--clean` fixture launched and stopped a scoped
  instance while leaving the persistent record intact.
- Abrupt-exit recovery, exact-PID crash report association, and bounded
  lifecycle waiting passed against live macOS processes. Evidence is in
  `.context/crash-recovery/evidence.json` and the private lease event log.
- Cycle 1 and Cycle 2 smoke wrappers are thin aliases. Fixture lists, optional
  visual assertions, and reset settings live in declarative manifests.
- App-side screenshot fixtures conservatively hold the foreground lease; a
  conflict test proves they fail before sending a socket command.
- Broad Cycle 1 smoke: 25 ordinary fixtures plus the expected-failure fixture
  passed; 9 older fixtures failed because they request archived preset names
  such as `calming-keys` and `african-horn`. Broad Cycle 2 smoke: 10 passed,
  while `graph-editing`, `pointer`, and `ported-presets` failed existing graph/UI
  assertions against the current app. Relative graph fixture paths now resolve
  from their fixture directory; `audio-capture` passed after that correction.

## Final Acceptance (2026-10-19)

- With Accessibility access granted, the focused Cycle V2 native `authoring`
  sequence passed through the coordinator's exact PID, instance lease, and
  foreground lease. It covered native node drag, palette creation, connection,
  undo/redo, save/reopen, deletion, and cable insertion.
- The Cycle V2 `opengl-diagnostics` foreground visual smoke passed with OS
  screenshot capture enabled.
- `--json` startup now keeps CMake build output on stderr, leaving stdout as
  one parseable result. The native smoke exposed this output contract failure.
- The native authoring fixture now selects the Nodes sidebar explicitly and
  inserts the global Delay on the global signal path, where graph validation
  permits it. Its previous per-voice insertion was a known fixture error. It
  also closes any expanded editor left by saved UI state before native input.
- A repeat attempt overlapped a Spokane-worktree direct Cycle V2 runner that
  does not use this coordinator. That other app took the foreground during a
  drag; its editor appears in `.context/native-authoring-captures/authoring-drag-stalled.png`.
  Machine-wide exclusion applies to callers that acquire the coordinator lease.
- The coordinator's 14 focused Python tests, semantic code map check, and
  Cycle V2 architecture audit passed. Broad-suite fixture failures listed
  above remain tracked separately in `docs/TDD/ui-bugs.md`.

## Ownership Follow-up (2026-10-19)

- A workspace/product build lease now spans the build, executable fingerprint,
  and instance selection. Fresh suite child runs acquire that lease individually.
- The PID-window screenshot helper crops requested regions from its selected
  window. A live Cycle V2 capture returned one PID and window ID for both the
  full image and a 400×300 region; the region had the requested pixel size.
  Untitled utility windows are excluded before enforcing a unique main window.
- Launch cleanup removes a socket only when its inode matches the recorded
  socket; a replaced or unknown socket remains a collision.
- Seventeen focused Python tests pass, including build-lease, socket-replacement,
  and PID-region-capture checks.
