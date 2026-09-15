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
| Per-step context | Each execution rebuilds maximum-capacity input/output metadata, `String` fields, attachments, output-presence scans, and route resolution. | Prebind immutable prepared context/routes once; block work patches only frame, timing, voice, and payload pointers. |
| Default modulation | Every voice/block scans every plan buffer and performs configuration downcasts. | Compile only actual modulation bindings; iterate `O(M)` rather than `O(B)`. |
| Spectral transfer | Every spectral input/voice/block repeats downcasts and scans Pan steps. | Resolve transfer bindings in prepared execution state; block lookup is `O(1)` per connected input. |
| Released voice tail | Every released voice/block scanned all plan steps and resolved processors. | Implemented in slice 2: direct prepared tail processor references make the query `O(T)`. |
| MIDI scheduling | Every callback sorts all scheduled/future events and moves the remaining suffix. | Do no sort without new events; measure queue depth and moves, then use sorted merge or a bounded heap if the distribution justifies it. |
| Realtime arenas | Every graph buffer reserves block plus `F*max(F,C)` grid payload in both work and voice-mix arenas. | Separate diagnostic grid storage from realtime block storage; allocate mix storage only for voice/global boundary buffers, then consider lifetime-based slot reuse. |
| Output path | Output is cleared before a valid overwrite, copied from the graph boundary, ramped/clipped, then scanned repeatedly for RMS/finiteness/peak. | The redundant L1 reduction and scratch copy/absolute/max passes are removed in slice 1 using `Buffer::minmax`; output clear/copy remain for a later bounded change. |
| Live capture | Inactive capture performs an atomic callback-counter RMW every callback. | Gate inactive capture before RMW or reuse the renderer callback id. |
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
