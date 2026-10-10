# Cycle 2 two-input port order

Status: implemented.

`NodePortLayout` owns rotator states; `NodeCanvasScene::portWorldCentre` owns socket and cable geometry; `GraphCommandDispatcher::editNodePresentation` owns undoable layout edits. The authored `Node::inputs` vector and edge port IDs carry routing semantics and must retain their order and identity.

Add a fifth side layout immediately after the normal left-input side layout. It reverses only the screen order of the two input sockets. Persist that presentation choice on the node, and have the existing scene geometry place each socket and its attached cable. The output remains on the right. Expose the rotator for nodes with exactly two inputs and an output, including Add, Multiply, and Stereo Join. Other layouts clear the reversed order.

Pointer action and commit retain the current `GraphCommandDispatcher` path. A click performs one presentation edit; it does not change graph topology, port IDs, DSP input order, or audio resources. Geometry lookup remains proportional to the local port count, independent of unrelated graph size.

Completion: prove the new state cycles directly from normal side layout, moves the two input sockets and cable endpoints without changing edge identities, survives serialization, and supports undo/redo. The rotator's icon must distinguish the reversed position. No parallel socket-position policy is allowed outside `NodeCanvasScene`.

Size review: `NodeCanvasPresentation.cpp` grows from 1,447 to 1,458 lines while retaining operation-button painting with its existing chrome code. Extract the action painters into a canvas-chrome renderer and delete the old helpers when that boundary is next refactored. `GraphSerializer.cpp` grows from 1,240 to 1,252 lines; extract node presentation fields (port sides and input order) into a dedicated codec and delete their inline serializer helpers in a focused serialization refactor. `NodeGraph.h` grows by one presentation flag, from 441 to 442 lines; node shape remains the shared graph contract. `NodeCanvasAuthoring.cpp` shrinks by removing a duplicate node-kind gate. Policy ownership remains with `NodePortLayout` for eligibility/state and `NodeCanvasScene` for geometry; authoring and painting ask those owners instead of restating the rule.

Verification: the focused `[layout]` tests pass 245 assertions in 15 cases, including state sequence, socket/cable geometry, Stereo Join action availability, serialization, and undo/redo. The reversed state is represented by crossed lines in the existing rotator icon. No temporary layout path or duplicate port ordering policy remains.
