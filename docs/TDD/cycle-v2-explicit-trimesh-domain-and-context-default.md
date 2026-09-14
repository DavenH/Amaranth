# Cycle V2 Explicit Trimesh Domain And Context Default

Status: Complete

## Scope

Make Trimesh signal semantics authored and visible without committing Cycle V2
to undefined multiple-oscillator behavior:

- persist each Trimesh signal type as Time, Magnitude, or Phase;
- persist magnitude polarity independently as Unipolar or Bipolar;
- remove Trimesh and Pan `auto`/`additive`/`multiplicative` mode parameters;
- let explicit Add and Multiply nodes determine arithmetic independently of
  magnitude polarity;
- infer one graph-wide Voice Context for cycle generators when exactly one
  Voice Context exists, without durable context edges;
- require explicit context assignment when more than one Voice Context exists,
  while rejecting graphs that activate more than one context;
- stop Cycle 1 conversion from creating redundant single-context source edges;
  and
- migrate existing graphs to explicit equivalents of their legacy authored
  combinations.

## Parked Proposal: Multiple Oscillators

Multiple Voice Contexts represent independent oscillators, but cross-context
cycle and spectral arithmetic is not yet defined. Spectral multiplication at
different pitches has no agreed bin/phase alignment contract. A future design
may introduce a per-synth-voice, post-oscillator combination phase where the
materialized outputs of independent contexts can be added or amplitude-
modulated before different polyphonic voices are mixed.

This implementation does not approximate that behavior. At most one Voice
Context may participate in oscillator regions. Additional Voice Context nodes
may exist but remain inactive until explicitly selected in a multi-context
graph; selecting a second context is a validation error.

## Semantic Contract

### Trimesh type

`signalType` is authoritative for the Trimesh output domain:

| Value | Output domain | Default polarity |
| --- | --- | --- |
| `time` | Time | Bipolar |
| `spectralMagnitude` | Spectral magnitude | Unipolar |
| `spectralPhase` | Spectral phase | Bipolar |

The shared three-segment pill edits this durable parameter through
`GraphCommandDispatcher`. Changing type preserves existing cables so ordinary
validation can identify incompatible destinations; it does not silently rewire
the graph.

### Magnitude polarity and arithmetic

`polarity` is visible only for spectral-magnitude Trimeshes and is independent
of downstream arithmetic. It controls the authored transfer semantics and the
compact and expanded render scale. Add and Multiply remain explicit graph
operations.

The mature audio raster remains unipolar for spectral magnitude, irrespective
of the authored polarity. Time and phase rasters remain bipolar. Applying
`polarity` directly to the magnitude raster before `SpectralLayerCore` range
shaping maps stored mesh values from `[0, 1]` to `[-1, 1]`; the established
multiplicative transfer then clips the negative half and can compound into
near-silence across several layers. Operation-specific polarity transfer is a
consumer-edge concern and is intentionally not approximated at the source.

Operation-specific spectral range normalization, disabled identity, and stereo
pan neutral handling are derived from the downstream Add or Multiply node, not
from polarity. All four combinations are legal:

- unipolar Add;
- unipolar Multiply;
- bipolar Add; and
- bipolar Multiply.

The compiler authors a transfer on each consumer edge. It combines source
polarity with the receiving Add or Multiply operation without mutating or
rescaling the source grid. This permits one Trimesh to feed multiple arithmetic
consumers while each receives the correct operand range. Audio keeps the mature
Cycle 1 unipolar magnitude raster and applies operation-derived range shaping
at the consumer boundary.

Let `x` be the canonical Trimesh magnitude in `[0, 1]`, `d` the mature
`magnitudeDynamicRange(range)`, `q = x^d`, `s = 2^d`, and `a` the mature
harmonic-count additive scale. The compiled input transfer is:

| Consumer | Polarity | Operand transfer |
| --- | --- | --- |
| Add | Unipolar | `q * s * a` |
| Add | Bipolar | `(q * s - 1) * a` |
| Multiply | Unipolar | `q` |
| Multiply | Bipolar | `q * s` |

This preserves both Cycle 1 combinations exactly. Bipolar transfers place the
authored midpoint at the receiving operation's identity: zero for Add and one
for Multiply. A disabled source is resolved at the same boundary to zero for
Add or one for Multiply, so one disabled source can safely feed unlike
consumers. Spectral pan is applied after the transfer, using additive gain or
multiplicative interpolation toward one as appropriate.

The compiled input retains graph provenance while optionally reading the
canonical Trimesh buffer through transparent Pan and single-input arithmetic
nodes. That lets divergent consumers receive independent transfer and pan
semantics without cloning the source mesh, mutating its buffer, or adding
durable graph nodes.

Cycle 1 migration emits only its authored combinations: additive layers become
Unipolar feeding Add, and filtering layers become Bipolar feeding Multiply.
Phase layers are Bipolar and feed Add. Time layers are Bipolar.

### Voice Context assignment

- With exactly one Voice Context, every cycle-generator source without an
  explicit context edge uses that context in the compiled plan. No implicit
  edge is added to `NodeGraph` or serialized output.
- With more than one Voice Context, no default is inferred. Participating cycle
  generators require explicit context edges.
- A multi-context graph may activate only one distinct Voice Context. Edges
  from a second context to any cycle generator are rejected until the parked
  oscillator-combination boundary is designed.
- Unconnected extra Voice Contexts compile as inactive configuration nodes and
  do not create oscillator regions.

The existing compiler-owned `GraphStepInput`/oscillator-region relationship is
the narrow boundary for inferred ownership. Domain resolution must not inspect
Voice Context parameters or traverse downstream consumers for Trimesh type.

## Migration

`GraphSerializer` accepts legacy graphs and translates before normalization:

- infer a missing Trimesh `signalType` from its legacy context/downstream
  resolved domain;
- translate Trimesh `spectralMode=additive` to `polarity=unipolar` and
  `spectralMode=multiplicative` to `polarity=bipolar`;
- resolve legacy `auto` from its downstream Add/Multiply operation;
- discard legacy Voice Context `domain`, Trimesh `spectralMode`, and Pan `mode`;
  and
- remove redundant Voice Context-to-cycle-generator edges when the graph has
  exactly one Voice Context.

The Cycle 1 converter emits the stable representation directly. An in-place
factory migration uses the serializer/converter-owned rules rather than a
second semantic implementation. User-modified preset files are not overwritten
by an automated batch.

## Authoritative Implementations

- `GraphDomainResolver` owns concrete signal-domain propagation.
- `GraphValidator` owns legal port/domain connections and the temporary
  one-active-context limit.
- `GraphCompiler` owns implicit single-context lowering and oscillator-region
  assignment.
- `TrimeshBlockwiseDsp` owns mature mesh point scaling. Preview renderers pass
  authored polarity for semantic display; audio renderers retain its native
  unipolar magnitude raster and consume it through the compiled operation
  transfer boundary.
- `SpectralLayerCore` retains the mature range, pan, and neutral-identity math.
- `GraphRenderSemanticResolver` and `TrimeshRenderProfile` remain the shared
  compact/expanded visual contract.
- `PropertySegmentedSelector` remains the shared pill implementation.
- `port_cycle_v1_preset.py` remains authoritative for Cycle 1 conversion.

## Complexity

Parameter reads and single-context lookup are O(1) per node after compile-time
graph scans. Transfer lowering walks only the transparent upstream chain of
each spectral arithmetic input. Live selector edits use the existing parameter
command path and do not clone a graph, serialize state, or scan mesh vertices.
Each consumer transforms only its own operand buffer with preallocated vector
operations; DSP work remains unchanged in asymptotic cost and allocation
behavior.

## Deletion Targets

- Voice Context `domain` parameter and domain selector.
- downstream spectral-domain inference for Trimesh.
- Trimesh `spectralMode` and its Auto/Add/Multiply selector.
- Pan `mode` persistence and configuration inference.
- converter-authored single-context source edges.

## Completion Criteria

- [x] New Trimeshes expose Type and applicable Polarity pills and publish each
  edit through one undoable command.
- [x] Trimesh output domains come only from `signalType`; incompatible existing
  cables remain visible and fail validation.
- [x] Polarity and Add/Multiply are independently represented in all four
  combinations without changing the mature magnitude-raster input range.
- [x] The compiler emits a per-consumer magnitude transfer for every
  polarity/arithmetic combination, including divergent consumers of one
  Trimesh source.
- [x] One Voice Context is inferred without durable source edges.
- [x] Multi-context graphs require explicit assignment and reject a second
  active context.
- [x] Legacy graph loading and Cycle 1 conversion map the two historical
  magnitude combinations and phase/time behavior to explicit semantics.
- [x] Clean factory presets contain no redundant single-context source edges or
  removed mode/domain parameters; user-modified presets remain untouched.
- [x] Focused tests, standalone build, native automation, production-size
  screenshots, style checks, and `git diff --check` pass.

## Verification

- `[graph]`: 177 cases, 3778 assertions.
- `[presets]`: 16 cases, 485 assertions.
- Cycle 1 converter and simplifier: 59 Python tests.
- The focused signal-semantics automation fixture completed 17 commands with
  no failures and captured the production-size expanded editor.
- The standalone Debug target builds successfully. The direct macOS native
  smoke retry was blocked by the existing System Events focus error `-10006`;
  the application-hosted pointer/undo fixture passed.
- The Baroque Flute live-audio regression capture passes at 0.092 peak and
  0.040 RMS; its fixture rejects output below 0.01 peak or 0.005 RMS.
- The Mind Blinding regression verifies two compiled bipolar Multiply inputs
  and a finite, non-silent render.
- The complete CTest matrix passes all 1,048 discovered tests in 198.23 seconds.
