# Cycle V2 Update Runtime Boundaries

## Status

Complete.

## Goal

Keep graph presentation orchestration readable and remove latency and lifetime
hazards from gesture and asynchronous update boundaries.

## Design

- A transient gesture copies its structural graph once but performs no XML
  serialization on mouse-down. Undo serialization, while XML persistence
  remains, occurs only at durable commit.
- Async job ownership, cancellation, message-thread publication, and lifetime
  fencing live behind a focused runtime component rather than raw callbacks in
  `GraphPresentationModel`.
- Fingerprints use one shared typed builder.
- Realtime request exchange performs bounded retries and returns “no request”
  rather than spinning behind a preempted writer.
- Infinite worker shutdown waits use a named lifetime-safety constant.

## Verification

Tests cover zero gesture-start serialization, coherent bounded request reads,
safe destruction with queued publication, and unchanged causal traces.

## Completion Criteria

The boundaries above are explicit, tested, style-clean, and pass the complete
Cycle V2 unit and native-edit suites.
