# Cycle 2 panel trackpad navigation

Status: implemented, including trackpad gesture tuning.

The shared `ZoomPanel` owns viewport bounds, scrollbar clamping, and viewport notifications. Cycle 2's `PanelInputHostComponent` owns routing of pointer input to the legacy panel interactor. The interactor's vertical wheel behavior remains authoritative for zoom, including its modifiers and tool handling.

Route horizontal wheel movement from Cycle 2 hosts to `ZoomPanel`'s horizontal scrollbar range. Route vertical movement to the interactor. A pure horizontal gesture must not trigger zoom or tool changes. Panning is constant work per wheel event and never changes the document graph or mesh. The shared interactor applies proportional zoom to smooth wheel events in both Cycle versions; discrete wheel steps retain their existing size.

Completion: a panel event regression proves horizontal panning preserves zoom width, vertical scrolling changes zoom width, and horizontal movement respects range limits. Remove any temporary routing or duplicate viewport clamping introduced during implementation.

Trackpad tuning: smooth wheel gestures accumulate a small initial movement before choosing one axis until a short pause, with a slight preference for horizontal movement so finger drift does not cause zoom. Smooth vertical zoom uses the wheel distance to scale the mature `ZoomPanel` zoom operation. Discrete wheel events retain their established step size. The axis choice is local to each panel input host and does not alter document state.

Size review: `Interactor.cpp` is 2,560 lines and already owns tool-specific wheel dispatch. This change adds only the smooth zoom factor within that handler. A later extraction should move wheel tool routing into a dedicated input policy and delete the old handler; splitting it here would expand the scope beyond trackpad behavior. `PanelInputHostComponent` owns only gesture direction selection, `Interactor` owns tool behavior, and `ZoomPanel` owns viewport geometry.

Verification: focused Trimesh panel event coverage includes the 2D spectral viewport and the 3D viewport after zooming, mixed-axis trackpad movement, and repeated smooth vertical events. The existing scrollbar owns clamping and notifications.
