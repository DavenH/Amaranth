# Cycle V2 UI Reliability Train

Status: In progress. Clean editor-open and safe document-replacement slices
completed 2026-09-25.

Architecture review after safe replacement: `Main.cpp` is 515 lines and owns
JUCE application composition, menu/dialog routing, and window presentation.
The new lifecycle policy is not retained there: the 117-line
`GraphDocumentReplacement` implementation and interface own pending-target,
decision, and save-completion state, leaving one replacement decision site for
the inline browser, full browser, file chooser, and recent-file menu.

Envelope release audit: the mature interactor and inverse graph delta already
restore the serialized mesh exactly. The reported mismatch was the native
fixture's 100 ms observation deadline racing asynchronous undo publication.
The fixture now uses a bounded 600 ms wait for this sequence and compares every
serialized float with zero tolerance; four consecutive runs passed. No
production mesh or undo behavior was duplicated or changed.

## Scope

This train repairs five deterministic or sequence-reproducible UI failures:

1. opening a graph can replace a dirty document without a save, discard, or
   cancel decision;
2. opening an Envelope editor can mark a clean document dirty;
3. undo after a routed Envelope release gesture does not restore the exact
   authored mesh;
4. a Reverb editor rebound after undo can retain a preview from a no-op
   gesture; and
5. a routed Waveshaper drag can manipulate a vertex other than the acquired
   target.

## Authority And Ownership

- `GraphDocument` owns durable graph content, save points, and undo history.
  `GraphDocument::isDirty()` is the only replacement-safety fact.
- `MainWindow` owns application file-dialog presentation, but replacement
  decisions belong in a small document-lifecycle collaborator shared by every
  interactive open entry point.
- `NodeCanvasAuthoring` owns editor selection and open state. Opening an editor
  is presentation-only; it must not publish graph parameters or editor state.
- Existing Envelope and Waveshaper widgets, interactors, and graph commands
  remain authoritative for acquisition, semantic deltas, and undo. Adapters
  may translate routed events but must not copy those interaction algorithms.
- The Reverb editor model is authoritative for durable parameters. Preview
  state must be rebound from that model after undo and must not outlive a
  canceled or unchanged gesture.

## Interaction And Complexity Contracts

- Opening an editor is O(1) in document size and performs no graph copy,
  serialization, publication, preparation, or undo capture.
- A dirty replacement prompts exactly once. Save continues replacement only
  after a successful save; Discard continues without saving; Cancel leaves the
  graph, file, dirty state, selection, and editor intact.
- Envelope and Waveshaper pointer-down acquisition is no more expensive than
  the mature interactor path. Movement scales only with the affected mesh
  delta and local render product.
- Undo applies the inverse semantic delta and restores serialized model state
  exactly; it does not snapshot unrelated graph or resource content.
- Reverb preview rebinding does not compile topology or recreate unchanged
  static resources.

## Completion Evidence

- A clean graph remains clean after opening and closing an Envelope editor.
- Interactive replacement covers Save, Discard, Cancel, failed save, recent
  files, the preset browser, and the file chooser through one policy boundary.
- A native Envelope release sequence performs at least two movement updates,
  commits, produces an observable model change, undoes, and matches the initial
  serialized mesh exactly.
- The Reverb size/undo/reopen/damping sequence leaves preview and durable
  parameters aligned after the no-op gesture and subsequent undo.
- The Waveshaper fixture asserts hovered vertex identity before mouse-down and
  proves that the same vertex moves while peer vertices remain stable.
- Focused tests and fixtures pass, followed by the Cycle V2 architecture audit,
  touched-file size review, `git diff --check`, and applicable build/tests.

## Deletion Targets

- Remove editor-open morph synchronization and its helper once preview
  selection changes are the only path that persists mapped morph values.
- Remove direct interactive graph loads from `MainWindow`; all such loads must
  pass through the replacement policy.
- Remove any stale local preview or gesture state identified as the cause of
  the Reverb sequence failure rather than masking it during paint.
