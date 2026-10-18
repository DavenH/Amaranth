---
name: cycle-performance-audit
description: Measure and diagnose Cycle V2 UI, rendering, or audio latency with representative workloads, telemetry, and operation counts. Use for performance audits or optimization requests, not ordinary UI visual review.
---

# Cycle Performance Audit

Read the performance sections of `docs/cycle-agent-runbook.md` and the relevant `docs/TDD/` performance document before choosing a measurement. Preserve the realtime and interaction-complexity rules in `AGENTS.md`.

1. Define the user's slow action as a reproducible workload: preset, graph size, event sequence, polyphony if audio is involved, build mode, cache state, and the exact latency endpoint. Include a representative ordinary workload and a scaled unrelated-state case when cost growth is in question.
2. Instrument before optimizing. Use `resetCanvasPerformance` / `inspectCanvasPerformance` for canvas work, or `resetAudioPerformance` / `inspectAudioPerformance` with stable MIDI workload for audio. Reuse an existing focused fixture when possible. Record both timing distributions and operation counts; identify the stage and owner that consume the time.
3. Compare like conditions: the same build type, preset contents, window size, workload, cache state, and machine. Follow the runbook's Debug/Release and fixture guidance where relevant. Do not turn a single timing or cross-machine millisecond threshold into a regression claim.
4. If optimization is requested, change the authoritative owner, then rerun the same workload and semantic checks. For live interaction, prove unrelated graph or domain growth does not increase movement work. For audio, keep telemetry opt-in and its callback path allocation-free, lock-free, and free of formatting.

Report baseline measurements by stage, after measurements and operation-count changes if code changed, semantic test results when applicable, and the remaining bottleneck or measurement limit. For an audit-only request, stop after evidence-backed findings and recommendations.
