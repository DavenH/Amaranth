# Cycle V2 Audio Performance

## Status

In progress (2026-09-15).

## Problem

Cycle V2 exposes distributions and operation counts for visual work, but its
realtime audio diagnostics contain only cumulative callback count and output
levels. The audio callback therefore has no safe phase timing, deadline
utilization, or workload history with which to distinguish executor overhead
from required DSP.

The current pipeline also repeats graph-wide work at block and voice scope.
Several operations are linear or worse in unrelated plan, processor, or arena
size even when the semantic delta is one audio block for one prepared voice.
These are correctness-level complexity regressions where an equivalent
prepared lookup or lifecycle-owned operation can be constant time.

## Authoritative Implementations And Boundaries

- `StandaloneAudioEngine` owns the device callback, graph publication/adoption,
  the existing 30 Hz non-realtime service point, and the live-capture boundary.
- `RealtimeGraphRenderer` owns MIDI scheduling, voice lifecycle, voice/global
  graph execution, output conditioning, and output meters.
- `GraphCompiler`, `GraphExecutionPlan`, and `GraphAudioExecutor` remain the
  authorities for graph order, routing, processor ownership, preparation, and
  execution. Telemetry observes these paths; it must not reproduce them.
- Mature oscillator rasterizers, FFT/IFFT, convolution, resampling, and
  oversampling implementations remain authoritative. They may be changed only
  after measured evidence isolates their own cost.
- `PerformanceDistribution` remains the shared non-realtime duration
  aggregation and JSON representation.

The only new boundary adapter is a fixed-size audio callback sample passed from
the renderer to an engine-owned collector. It translates realtime counters and
timestamps into an SPSC handoff. It contains no DSP, graph, voice, or scheduling
behavior. Its stable end state is a permanent instrumentation boundary rather
than a compatibility layer.

## Thread And Lifecycle Design

Audio performance collection is opt-in. `resetAudioPerformance` starts a fresh
enabled measurement generation. The audio thread reads the enable/generation
state once per callback, writes timestamps and plain counters into one
preallocated callback-owned POD sample, then attempts one bounded push into a
preallocated SPSC ring.

`StandaloneAudioEngine::timerCallback()` is the sole consumer. It drains the
ring and updates distributions, percentiles, and JSON-ready aggregates off the
audio thread. A reset advances an atomic generation and clears non-realtime
aggregates. Samples from an older generation are discarded when drained.

The producer never allocates, locks, formats text, creates `juce::var`, waits,
or retries. A full ring drops telemetry only and increments an observable drop
counter. Instrumentation cannot back-pressure audio or change rendering.

## Audit And Cost Contracts

| Operation | Current cost | Required cost and disposition |
| --- | --- | --- |
| Callback observability | Aggregate atomics only; no phase or deadline history. | `O(1)` bounded POD publication per enabled callback; implement first. |
| Processor retirement | `removeUnreferencedProcessors()` ran after every active voice and global pass, scanning processors, prepared voices, and each voice processor list. | Implemented in slice 2: retirement runs after preparation, before publication; callback cleanup is `O(1)`. |
| Scope filtering | Every active voice and the global pass scanned all execution steps and rejected the other scope. | Implemented in slice 2: prepared voice/global step indices execute `O(A*Svoice + Sglobal)`, independent of unrelated scope steps. |
| Per-step context | Each execution rebuilt maximum-capacity input/output metadata, `String` fields, attachments, output-presence scans, and route resolution. | Implemented in slice 3c: preparation owns routed contexts; block work patches frame, timing, voice, capture mode, and output state only. |
| Default modulation | Every voice/block scanned every plan buffer and performed configuration downcasts. | Implemented in slice 3a: preparation stores typed source/buffer bindings; block work is `O(M)` rather than `O(B)`. |
| Spectral transfer | Every executor spectral input/voice/block repeated downcasts and scanned Pan steps. | Implemented in slice 3b: preparation resolves transfer values; executor block lookup is `O(1)` per connected input. Oscillator-frame transfer work remains under its authoritative preparation path. |
| Released voice tail | Every released voice/block scanned all plan steps and resolved processors. | Implemented in slice 2: direct prepared tail processor references make the query `O(T)`. |
| MIDI scheduling | Every callback sorted all scheduled/future events and moved the remaining suffix. | Slice 4a skips sorting when no new events arrived and reports dequeued, sorted-item, and compacted-item totals. Retained future events remain sorted. |
| Realtime arenas | Every graph buffer reserves block plus `F*max(F,C)` grid payload in both work and voice-mix arenas. | Separate diagnostic grid storage from realtime block storage; allocate mix storage only for voice/global boundary buffers, then consider lifetime-based slot reuse. |
| Output path | Output is cleared before a valid overwrite, copied from the graph boundary, ramped/clipped, then scanned repeatedly for RMS/finiteness/peak. | The redundant L1 reduction and scratch copy/absolute/max passes are removed in slice 1 using `Buffer::minmax`; output clear/copy remain for a later bounded change. |
| Live capture | Inactive capture performed an atomic callback-counter RMW every callback. | Slice 4b passes the renderer callback id into capture; inactive capture now performs only its target-state load. |
| MIDI controls | All 128 controller values are copied into every active voice every block. | Share an immutable block snapshot or compile only referenced controllers. |

`A` is active voices, `Svoice`/`Sglobal` are compiled steps of that scope,
`B` is plan buffers, `M` is modulation bindings, `F` is maximum audio frames,
and `C` is traversal columns.

## Implementation Slices

1. Add the lock-free callback handoff, phase/deadline distributions, workload
   counts, reset/inspection automation, and focused contract tests. Record a
   stable 0/1/4/8-voice baseline.
2. Move processor retirement out of block execution and compile scope-specific
   step lists. Add operation counters and scaling tests proving unrelated graph
   scope/processor growth does not increase callback work.
3. Prebind step contexts, modulation bindings, spectral transfers, and tail
   processors. Add prepared-path allocation and lookup-count tests.
4. Measure MIDI depth/moves, arena high-water/storage, output passes, capture
   activity, and controller references; implement only the measured structural
   wins with independent semantic tests.
5. Profile core DSP under representative chained, spectral, and global-effect
   graphs. Open focused TDDs for any authoritative algorithm whose measured
   cost warrants a behavioral or architectural change.

## Telemetry Schema

The first schema reports callback and deadline distributions, overruns,
telemetry drops, frame/voice workload totals and maxima, and these phases:

- graph adoption;
- output clear and block setup;
- MIDI scheduling;
- active voice rendering;
- global graph rendering;
- output copy/conditioning;
- meter publication;
- live capture.

The callback sample also carries graph revision, frame count, active voice
count, scheduled MIDI count, and compiled execution-step count. Later slices
add operation counters without changing the handoff ownership.

## Expected Production Change

The instrumentation slice should add one runtime collector pair, narrow timing
hooks in `StandaloneAudioEngine` and `RealtimeGraphRenderer`, and four small
automation forwarding methods. The collector should remain below roughly 350
production lines and renderer/engine changes below roughly 150 lines. A larger
adapter, DSP logic in the collector, or node-kind branching is evidence that
the boundary is wrong.

## Negative Boundaries

- No mutex, allocation, logging, JSON, file I/O, wait, retry, or unbounded loop
  may be added to the audio-thread telemetry path.
- Telemetry overflow must not alter MIDI, voice, graph, output, or capture state.
- Instrumentation must be disabled until explicitly reset/enabled and must add
  only one predictable branch to a disabled callback.
- Stateful audio output must not be memoized by graph revision alone.
- No DSP approximation, copied mature implementation, or speculative algorithm
  rewrite is accepted as an optimization.
- Timing tests may validate aggregation and schema, but complexity contracts
  require deterministic operation counters rather than timing thresholds.

## Completion Criteria

- Automation can reset/start and inspect audio performance using the live
  callback, with p50/p95/p99/max callback duration, deadline utilization,
  overruns, phase distributions, workload counts, and dropped samples.
- Focused tests prove SPSC ordering, bounded overflow, generation reset, schema,
  and unchanged audio output with telemetry enabled.
- A repeatable fixture captures 0/1/4/8-voice and representative spectral/global
  baselines and records artifacts here.
- Processor retirement, scope filtering, prepared bindings/context, tail query,
  MIDI scheduling, arenas, output passes, capture, and MIDI controls have either
  implemented complexity tests and before/after evidence or a documented
  measured reason to retain their present design.
- Modified DSP/realtime files pass the hot-loop math self-check, applicable
  focused tests, standalone Debug build, clang-tidy where available, and
  `git diff --check`.

## Deletion Targets And Stable End State

Delete callback-time reachability cleanup, whole-plan ownership filtering,
block-time configuration downcasts, redundant route/context assembly, and any
temporary counters superseded by the permanent telemetry schema. The stable
executor performs only work whose inputs change for the current block/voice;
preparation owns immutable bindings and storage, while the non-realtime service
owns aggregation, retirement, formatting, and publication.

## Slice 1 Result (2026-09-15)

The opt-in collector, preallocated SPSC handoff, nine phase timers, deadline
utilization/overrun reporting, workload counters, and automation commands are
implemented. Focused tests cover aggregation/schema, bounded overflow,
generation reset, disabled collection, zero realtime allocation/locking, and
bit-identical renderer output with telemetry attached.

The output meter now derives peak from the existing vectorized minimum/maximum
reduction. This removes the L1 finite-check reduction, the 8,192-float scratch
buffer, and the copy/absolute/max passes. The focused meter phase fell from
0.0035 to 0.0022 ms at zero voices and from 0.0031 to 0.0020 ms at one voice;
at four/eight voices it was already roughly 0.0012 ms and remained there.

The stable Baroque Flute fixture measured 512-frame callbacks at 44.1 kHz in a
Debug standalone build:

| Active voices | Callback mean | p95 bucket | Maximum | Mean deadline use | Voice phase mean |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | 0.584 ms | 1 ms | 0.671 ms | 5.0% | 0.005 ms |
| 1 | 2.312 ms | 4 ms | 3.129 ms | 19.9% | 1.780 ms |
| 4 | 3.773 ms | 4 ms | 6.391 ms | 32.4% | 3.573 ms |
| 8 | 6.762 ms | 8 ms | 7.649 ms | 58.2% | 6.564 ms |

All four windows reported zero deadline overruns and zero telemetry drops. The
idle global phase averaged 0.561 ms, while voice rendering became the dominant
phase under polyphony. This establishes the baseline for executor slices 2 and
3; it does not by itself attribute cost within a voice pass.

Artifacts:

- `/private/tmp/cycle-v2-audio-performance-baseline.json`
- `/private/tmp/cycle-v2-audio-performance-report.json`

## Slice 2 Result (2026-09-15)

Each prepared voice/global entry now owns its applicable step-index list and
tail-processor list. Realtime execution no longer filters the whole plan, and
tail queries no longer scan unrelated steps. Processor reachability cleanup
runs after preparation instead of after every voice/global pass. The telemetry
schema now reports prepared step visits per callback.

A scaling test appends 128 unrelated global steps and proves the voice pass
still visits only its two voice steps; the global pass visits its own prepared
steps. The plan-replacement test proves a stale processor is retired by
preparation without requiring a subsequent audio block. Existing realtime
allocation/lock and output-view contracts remain green.

On the same Baroque Flute fixture, the prepared plan has 16 voice steps and
three global steps. Telemetry reports exactly `3 + 16*A` visits for `A` active
voices. Compared with the immediately preceding build:

| Active voices | Callback mean before | Callback mean after | Voice phase before | Voice phase after |
| ---: | ---: | ---: | ---: | ---: |
| 0 | 1.132 ms | 0.860 ms | 0.004 ms | 0.004 ms |
| 1 | 2.562 ms | 2.383 ms | 1.984 ms | 1.965 ms |
| 4 | 3.748 ms | 3.355 ms | 3.550 ms | 3.243 ms |
| 8 | 6.793 ms | 6.009 ms | 6.593 ms | 5.894 ms |

The eight-voice callback mean fell 11.6%. All windows again reported zero
deadline overruns and zero telemetry drops. Debug timings remain noisy, so the
operation-count contract—not a timing threshold—is the regression guard.

Artifacts:

- `/private/tmp/cycle-v2-audio-performance-pre-executor.json`
- `/private/tmp/cycle-v2-audio-performance-report.json`

## Slice 3a Result (2026-09-15)

Preparation now resolves default modulation configurations once and stores only
the typed source, destination buffer index, and note offset needed by block
rendering. Realtime execution no longer scans all graph buffers or performs
configuration downcasts. Telemetry reports binding visits, and a scaling test
proves that adding 128 unrelated buffers leaves the visit count equal to the
actual modulation binding count.

The Baroque Flute fixture contains three default modulation bindings per active
voice. Live telemetry reported exactly `3*A` binding visits for `A` active
voices (0, 3, 12, and 24 across the four windows), with zero deadline overruns
and telemetry drops. Callback means were 0.921, 2.427, 3.370, and 5.980 ms.
This fixture has few unrelated buffers, so the timing change is within Debug
run noise; the operation-count contract captures the structural improvement.

Artifact: `/private/tmp/cycle-v2-audio-performance-modulation.json`.

## Slice 3b Result (2026-09-15)

Executor preparation now resolves spectral magnitude transfer configuration and
Pan chains into values indexed by step and input. A block copies the prepared
value for each active binding; it performs no configuration downcast or Pan
chain traversal. The scaling test gives one active transfer 128 inert Pan
indices and proves callback work remains one binding visit.

Baroque Flute telemetry reported zero executor spectral-transfer visits because
its spectral chain is materialized by the existing oscillator-region renderer,
not by per-block executor steps. Callback means remained within Debug run noise
at 0.884, 2.424, 3.431, and 5.950 ms, with zero overruns or telemetry drops.
The retained oscillator-region path should be profiled separately in slice 5
before changing its mature frame renderer.

Artifact: `/private/tmp/cycle-v2-audio-performance-spectral.json`.

## Slice 3c Result (2026-09-15)

Each prepared execution step now owns its routed `AudioProcessContext`,
including input/output payload pointers, output-port metadata, attachments,
spectral values, and output-presence classification. Per block, the executor
patches only frame count, timing, voice, traversal-capture mode, and the active
output list. A scaling test adds 128 unrelated routes to an Output step and
proves identical samples and an unchanged number of context patches.

Live Baroque Flute telemetry reports 3 context patches while idle and 8, 23,
and 43 at 1, 4, and 8 active voices. This is smaller than prepared-step visits
because oscillator-region members that do not materialize a block are skipped
before context patching. Compared with the preceding spectral-binding run,
callback means changed from 0.884/2.424/3.431/5.950 ms to
0.392/2.065/3.371/5.960 ms. The idle and one-voice reductions are clear; the
four/eight-voice difference is within Debug run noise because mature oscillator
DSP dominates those windows. No window reported an overrun or telemetry drop.

Artifact: `/private/tmp/cycle-v2-audio-performance-context.json`.

## Slice 4a Result (2026-09-15)

MIDI scheduling now preserves the sorted retained suffix without invoking
`std::sort` on callbacks where the queue contributed no new events. Telemetry
records dequeued events, sorted items, and compacted items so a later change to
sorted merge or a bounded heap can be justified by actual queue distributions.
The deferred-event sequence test proves the first callback sorts the new item,
the next callback performs zero sort work, and the event still activates its
voice at the intended future callback.

## Slice 4b Result (2026-09-15)

Cycle V2 now passes the renderer's existing callback sequence into
`AudioCallbackCapture`. The shared capture retains its self-counting entry point
for `AudioHub`, while the externally sequenced entry point checks capture state
before touching samples and performs no callback-counter RMW. Tests cover both
the inactive fast path and preserved first/last callback identifiers.
